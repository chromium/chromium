/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/third_party/mozilla_security_manager/nsPKCS12Blob.h"

#include <pk11pub.h>
#include <pkcs12.h>
#include <p12plcy.h>
#include <secerr.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/nss_util_internal.h"
#include "net/base/net_errors.h"

namespace mozilla_security_manager {

namespace {

// unicodeToItem
//
// For the NSS PKCS#12 library, must convert char16_ts (shorts) to
// a buffer of octets.  Must handle byte order correctly.
// TODO: Is there a Mozilla way to do this?  In the string lib?
void unicodeToItem(const char16_t* uni, SECItem* item) {
  int len = 0;
  while (uni[len++] != 0);
  SECITEM_AllocItem(NULL, item, sizeof(PRUnichar) * len);
#ifdef IS_LITTLE_ENDIAN
  int i = 0;
  for (i=0; i<len; i++) {
    item->data[2*i  ] = (unsigned char )(uni[i] << 8);
    item->data[2*i+1] = (unsigned char )(uni[i]);
  }
#else
  memcpy(item->data, uni, item->len);
#endif
}

// write_export_data
// write bytes to the exported PKCS#12 data buffer
void write_export_data(void* arg, const char* buf, unsigned long len) {
  std::string* dest = reinterpret_cast<std::string*>(arg);
  dest->append(buf, len);
}

// nickname_collision
// what to do when the nickname collides with one already in the db.
// Based on P12U_NicknameCollisionCallback from nss/cmd/pk12util/pk12util.c
SECItem* PR_CALLBACK
nickname_collision(SECItem *old_nick, PRBool *cancel, void *wincx)
{
  char           *nick     = NULL;
  SECItem        *ret_nick = NULL;
  CERTCertificate* cert    = (CERTCertificate*)wincx;

  if (!cancel || !cert) {
    // pk12util calls this error user cancelled?
    return NULL;
  }

  if (!old_nick)
    VLOG(1) << "no nickname for cert in PKCS12 file.";

  nick = CERT_MakeCANickname(cert);
  if (!nick) {
    return NULL;
  }

  if(old_nick && old_nick->data && old_nick->len &&
     PORT_Strlen(nick) == old_nick->len &&
     !PORT_Strncmp((char *)old_nick->data, nick, old_nick->len)) {
    PORT_Free(nick);
    PORT_SetError(SEC_ERROR_IO);
    return NULL;
  }

  VLOG(1) << "using nickname " << nick;
  ret_nick = PORT_ZNew(SECItem);
  if(ret_nick == NULL) {
    PORT_Free(nick);
    return NULL;
  }

  ret_nick->data = (unsigned char *)nick;
  ret_nick->len = PORT_Strlen(nick);

  return ret_nick;
}

// pip_ucs2_ascii_conversion_fn
// required to be set by NSS (to do PKCS#12), but since we've already got
// unicode make this a no-op.
PRBool
pip_ucs2_ascii_conversion_fn(PRBool toUnicode,
                             unsigned char *inBuf,
                             unsigned int inBufLen,
                             unsigned char *outBuf,
                             unsigned int maxOutBufLen,
                             unsigned int *outBufLen,
                             PRBool swapBytes)
{
  CHECK_GE(maxOutBufLen, inBufLen);
  // do a no-op, since I've already got Unicode.  Hah!
  *outBufLen = inBufLen;
  memcpy(outBuf, inBuf, inBufLen);
  return PR_TRUE;
}

// Based on nsPKCS12Blob::ImportFromFileHelper.
int nsPKCS12Blob_ImportHelper(const char* pkcs12_data,
                              size_t pkcs12_len,
                              const std::u16string& password,
                              bool is_extractable,
                              bool try_zero_length_secitem,
                              PK11SlotInfo* slot,
                              net::ScopedCERTCertificateList* imported_certs) {
  DCHECK(pkcs12_data);
  DCHECK(slot);
  int import_result = net::ERR_PKCS12_IMPORT_FAILED;
  SECStatus srv = SECSuccess;
  SEC_PKCS12DecoderContext *dcx = NULL;
  SECItem unicodePw;
  SECItem attribute_value;
  CK_BBOOL attribute_data = CK_FALSE;
  const SEC_PKCS12DecoderItem* decoder_item = NULL;

  unicodePw.type = siBuffer;
  unicodePw.len = 0;
  unicodePw.data = NULL;
  if (!try_zero_length_secitem) {
    unicodeToItem(password.c_str(), &unicodePw);
  }

  // Initialize the decoder
  dcx = SEC_PKCS12DecoderStart(&unicodePw, slot,
                               // wincx
                               NULL,
                               // dOpen, dClose, dRead, dWrite, dArg: NULL
                               // specifies default impl using memory buffer.
                               NULL, NULL, NULL, NULL, NULL);
  if (!dcx) {
    srv = SECFailure;
    goto finish;
  }
  // feed input to the decoder
  srv = SEC_PKCS12DecoderUpdate(dcx,
                                (unsigned char*)pkcs12_data,
                                pkcs12_len);
  if (srv) goto finish;
  // verify the blob
  srv = SEC_PKCS12DecoderVerify(dcx);
  if (srv) goto finish;
  // validate bags
  srv = SEC_PKCS12DecoderValidateBags(dcx, nickname_collision);
  if (srv) goto finish;
  // import certificate and key
  srv = SEC_PKCS12DecoderImportBags(dcx);
  if (srv) goto finish;

  attribute_value.data = &attribute_data;
  attribute_value.len = sizeof(attribute_data);

  srv = SEC_PKCS12DecoderIterateInit(dcx);
  if (srv) goto finish;

  if (imported_certs)
    imported_certs->clear();

  // Collect the list of decoded certificates, and mark private keys
  // non-extractable if needed.
  while (SEC_PKCS12DecoderIterateNext(dcx, &decoder_item) == SECSuccess) {
    if (decoder_item->type != SEC_OID_PKCS12_V1_CERT_BAG_ID)
      continue;

    net::ScopedCERTCertificate cert(
        PK11_FindCertFromDERCertItem(slot, decoder_item->der,
                                     NULL));  // wincx
    if (!cert) {
      LOG(ERROR) << "Could not grab a handle to the certificate in the slot "
                 << "from the corresponding PKCS#12 DER certificate.";
      continue;
    }

    // Once we have determined that the imported certificate has an
    // associated private key too, only then can we mark the key as
    // non-extractable.
    // Iterate through all the imported PKCS12 items and mark any accompanying
    // private keys as non-extractable.
    if (decoder_item->hasKey && !is_extractable) {
      SECKEYPrivateKey* privKey = PK11_FindPrivateKeyFromCert(slot, cert.get(),
                                                              NULL);  // wincx
      if (privKey) {
        // Mark the private key as non-extractable.
        srv = PK11_WriteRawAttribute(PK11_TypePrivKey, privKey, CKA_EXTRACTABLE,
                                     &attribute_value);
        SECKEY_DestroyPrivateKey(privKey);
        if (srv) {
          LOG(ERROR) << "Could not set CKA_EXTRACTABLE attribute on private "
                     << "key.";
          break;
        }
      }
    }

    // Add the cert to the list
    if (imported_certs)
      imported_certs->push_back(std::move(cert));

    if (srv) goto finish;
  }
  import_result = net::OK;
finish:
  // If srv != SECSuccess, NSS probably set a specific error code.
  // We should use that error code instead of inventing a new one
  // for every error possible.
  if (srv != SECSuccess) {
    int error = PORT_GetError();
    LOG(ERROR) << "PKCS#12 import failed with error " << error;
    switch (error) {
      case SEC_ERROR_BAD_PASSWORD:
      case SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT:
        import_result = net::ERR_PKCS12_IMPORT_BAD_PASSWORD;
        break;
      case SEC_ERROR_PKCS12_INVALID_MAC:
        import_result = net::ERR_PKCS12_IMPORT_INVALID_MAC;
        break;
      case SEC_ERROR_BAD_DER:
      case SEC_ERROR_PKCS12_DECODING_PFX:
      case SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE:
        import_result = net::ERR_PKCS12_IMPORT_INVALID_FILE;
        break;
      case SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM:
      case SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE:
      case SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM:
      case SEC_ERROR_PKCS12_UNSUPPORTED_VERSION:
        import_result = net::ERR_PKCS12_IMPORT_UNSUPPORTED;
        break;
      default:
        import_result = net::ERR_PKCS12_IMPORT_FAILED;
        break;
    }
  }
  // Finish the decoder
  if (dcx)
    SEC_PKCS12DecoderFinish(dcx);
  SECITEM_ZfreeItem(&unicodePw, PR_FALSE);
  return import_result;
}

// Attempt to read the CKA_EXTRACTABLE attribute on a private key inside
// a token. On success, store the attribute in |extractable| and return
// SECSuccess.
SECStatus
isExtractable(SECKEYPrivateKey *privKey, PRBool *extractable)
{
  SECItem value;
  SECStatus rv;

  rv=PK11_ReadRawAttribute(PK11_TypePrivKey, privKey, CKA_EXTRACTABLE, &value);
  if (rv != SECSuccess)
    return rv;

  if ((value.len == 1) && (value.data != NULL))
    *extractable = !!(*(CK_BBOOL*)value.data);
  else
    rv = SECFailure;
  SECITEM_FreeItem(&value, PR_FALSE);
  return rv;
}

class PKCS12InitSingleton {
 public:
  // From the PKCS#12 section of nsNSSComponent::InitializeNSS in
  // nsNSSComponent.cpp.
  PKCS12InitSingleton() {
    // Enable ciphers for PKCS#12
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
    SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);

    // Set no-op ascii-ucs2 conversion function to work around weird NSS
    // interface.  Thankfully, PKCS12 appears to be the only thing in NSS that
    // uses PORT_UCS2_ASCIIConversion, so this doesn't break anything else.
    PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);
  }
};

// Leaky so it can be initialized on worker threads and because there is no
// cleanup necessary.
static base::LazyInstance<PKCS12InitSingleton>::Leaky g_pkcs12_init_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void EnsurePKCS12Init() {
  g_pkcs12_init_singleton.Get();
}

// Based on nsPKCS12Blob::ImportFromFile.
int nsPKCS12Blob_Import(PK11SlotInfo* slot,
                        const char* pkcs12_data,
                        size_t pkcs12_len,
                        const std::u16string& password,
                        bool is_extractable,
                        net::ScopedCERTCertificateList* imported_certs) {
  int rv = nsPKCS12Blob_ImportHelper(pkcs12_data, pkcs12_len, password,
                                     is_extractable, false, slot,
                                     imported_certs);

  // When the user entered a zero length password:
  //   An empty password should be represented as an empty
  //   string (a SECItem that contains a single terminating
  //   NULL UTF16 character), but some applications use a
  //   zero length SECItem.
  //   We try both variations, zero length item and empty string,
  //   without giving a user prompt when trying the different empty password
  //   flavors.
  if ((rv == net::ERR_PKCS12_IMPORT_BAD_PASSWORD ||
       rv == net::ERR_PKCS12_IMPORT_INVALID_MAC) &&
      password.empty()) {
    rv = nsPKCS12Blob_ImportHelper(pkcs12_data, pkcs12_len, password,
                                   is_extractable, true, slot, imported_certs);
  }
  return rv;
}

// Based on nsPKCS12Blob::ExportToFile
//
// Having already loaded the certs, form them into a blob (loading the keys
// also), encode the blob, and stuff it into the file.
//
// TODO: handle slots correctly
//       mirror "slotToUse" behavior from PSM 1.x
//       verify the cert array to start off with?
//       set appropriate error codes
int nsPKCS12Blob_Export(std::string* output,
                        const net::ScopedCERTCertificateList& certs,
                        const std::u16string& password) {
  int return_count = 0;
  SECStatus srv = SECSuccess;
  SEC_PKCS12ExportContext *ecx = NULL;
  SEC_PKCS12SafeInfo *certSafe = NULL, *keySafe = NULL;
  SECItem unicodePw;
  unicodePw.type = siBuffer;
  unicodePw.len = 0;
  unicodePw.data = NULL;

  int numCertsExported = 0;

  // get file password (unicode)
  unicodeToItem(password.c_str(), &unicodePw);

  // what about slotToUse in psm 1.x ???
  // create export context
  ecx = SEC_PKCS12CreateExportContext(NULL, NULL, NULL /*slot*/, NULL);
  if (!ecx) {
    srv = SECFailure;
    goto finish;
  }
  // add password integrity
  srv = SEC_PKCS12AddPasswordIntegrity(ecx, &unicodePw, SEC_OID_SHA1);
  if (srv) goto finish;

  for (size_t i=0; i<certs.size(); i++) {
    DCHECK(certs[i].get());
    CERTCertificate* nssCert = certs[i].get();
    DCHECK(nssCert);

    // We only allow certificate and private key extraction if the corresponding
    // CKA_EXTRACTABLE private key attribute is set to CK_TRUE. Most hardware
    // tokens including smartcards enforce this behavior. An internal (soft)
    // token may ignore this attribute (and hence still be able to export) but
    // we still refuse to attempt an export.
    // In addition, some tokens may not support this attribute, in which case
    // we still attempt the export and let the token implementation dictate
    // the export behavior.
    if (nssCert->slot) {
      SECKEYPrivateKey *privKey=PK11_FindKeyByDERCert(nssCert->slot,
                                                      nssCert,
                                                      NULL);  // wincx
       if (privKey) {
        PRBool privKeyIsExtractable = PR_FALSE;
        SECStatus rv = isExtractable(privKey, &privKeyIsExtractable);
        SECKEY_DestroyPrivateKey(privKey);

        if (rv == SECSuccess && !privKeyIsExtractable) {
          LOG(ERROR) << "Private key is not extractable";
          continue;
        }
      }
    }

    // XXX this is why, to verify the slot is the same
    // PK11_FindObjectForCert(nssCert, NULL, slot);
    // create the cert and key safes
    keySafe = SEC_PKCS12CreateUnencryptedSafe(ecx);
    if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
      certSafe = keySafe;
    } else {
      certSafe = SEC_PKCS12CreatePasswordPrivSafe(ecx, &unicodePw,
                           SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }
    if (!certSafe || !keySafe) {
      LOG(ERROR) << "!certSafe || !keySafe " << certSafe << " " << keySafe;
      srv = SECFailure;
      goto finish;
    }
    // add the cert and key to the blob
    srv = SEC_PKCS12AddCertAndKey(ecx, certSafe, NULL, nssCert,
                                  CERT_GetDefaultCertDB(),
                                  keySafe, NULL, PR_TRUE, &unicodePw,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC);
    if (srv) goto finish;
    ++numCertsExported;
  }

  if (!numCertsExported) goto finish;

  // encode and write
  srv = SEC_PKCS12Encode(ecx, write_export_data, output);
  if (srv) goto finish;
  return_count = numCertsExported;
finish:
  if (srv)
    LOG(ERROR) << "PKCS#12 export failed with error " << PORT_GetError();
  if (ecx)
    SEC_PKCS12DestroyExportContext(ecx);
  SECITEM_ZfreeItem(&unicodePw, PR_FALSE);
  return return_count;
}

}  // namespace mozilla_security_manager
