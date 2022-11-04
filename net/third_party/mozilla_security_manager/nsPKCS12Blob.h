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

#ifndef NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSPKCS12BLOB_H_
#define NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSPKCS12BLOB_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "net/cert/scoped_nss_types.h"

typedef struct CERTCertificateStr CERTCertificate;
typedef struct PK11SlotInfoStr PK11SlotInfo;

namespace mozilla_security_manager {

// Initialize NSS PKCS#12 libs.
void EnsurePKCS12Init();

// Import the private key and certificate from a PKCS#12 blob into the slot.
// If |is_extractable| is false, mark the private key as non-extractable.
// Returns a net error code.  |imported_certs|, if non-NULL, returns a list of
// certs that were imported.
int nsPKCS12Blob_Import(PK11SlotInfo* slot,
                        const char* pkcs12_data,
                        size_t pkcs12_len,
                        const std::u16string& password,
                        bool is_extractable,
                        net::ScopedCERTCertificateList* imported_certs);

// Export the given certificates into a PKCS#12 blob, storing into output.
// Returns the number of certificates exported.
// TODO(mattm): provide better error return status?
int nsPKCS12Blob_Export(std::string* output,
                        const net::ScopedCERTCertificateList& certs,
                        const std::u16string& password);

}  // namespace mozilla_security_manager

#endif  // NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSPKCS12BLOB_H_
