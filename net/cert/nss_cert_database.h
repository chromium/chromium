// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_CERT_DATABASE_H_
#define NET_CERT_NSS_CERT_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/cert/cert_type.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"

namespace net {

// Provides functions to manipulate the NSS certificate stores.
// Forwards notifications about certificate changes to the global CertDatabase
// singleton.
class NET_EXPORT NSSCertDatabase {
 public:
  class NET_EXPORT Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    // Will be called when a certificate is added, removed, or trust settings
    // are changed.
    virtual void OnTrustStoreChanged() {}
    virtual void OnClientCertStoreChanged() {}

   protected:
    Observer() = default;
  };

  // Holds an NSS certificate along with additional information.
  struct CertInfo {
    CertInfo();
    CertInfo(CertInfo&& other);
    ~CertInfo();
    CertInfo& operator=(CertInfo&& other);

    // The certificate itself.
    ScopedCERTCertificate cert;

    // The certificate is stored on a read-only slot.
    bool on_read_only_slot = false;

    // The certificate is untrusted.
    bool untrusted = false;

    // The certificate is trusted for web navigations according to the trust
    // bits stored in the database.
    bool web_trust_anchor = false;

    // The certificate is hardware-backed.
    bool hardware_backed = false;

    // The certificate is device-wide.
    // Note: can be true only on Chrome OS.
    bool device_wide = false;
  };

  // Stores per-certificate error codes for import failures.
  struct NET_EXPORT ImportCertFailure {
   public:
    ImportCertFailure(ScopedCERTCertificate cert, int err);
    ImportCertFailure(ImportCertFailure&& other);
    ~ImportCertFailure();

    ScopedCERTCertificate certificate;
    int net_error;
  };
  typedef std::vector<ImportCertFailure> ImportCertFailureList;

  // Constants that define which usages a certificate is trusted for.
  // They are used in combination with CertType to specify trust for each type
  // of certificate.
  // For a CA_CERT, they specify that the CA is trusted for issuing server and
  // client certs of each type.
  // For SERVER_CERT, only TRUSTED_SSL makes sense, and specifies the cert is
  // trusted as a server.
  // For EMAIL_CERT, only TRUSTED_EMAIL makes sense, and specifies the cert is
  // trusted for email.
  // DISTRUSTED_* specifies that the cert should not be trusted for the given
  // usage, regardless of whether it would otherwise inherit trust from the
  // issuer chain.
  // Use TRUST_DEFAULT to inherit trust as normal.
  // NOTE: The actual constants are defined using an enum instead of static
  // consts due to compilation/linkage constraints with template functions.
  typedef uint32_t TrustBits;
  enum {
    TRUST_DEFAULT         =      0,
    TRUSTED_SSL           = 1 << 0,
    TRUSTED_EMAIL         = 1 << 1,
    TRUSTED_OBJ_SIGN      = 1 << 2,
    DISTRUSTED_SSL        = 1 << 3,
    DISTRUSTED_EMAIL      = 1 << 4,
    DISTRUSTED_OBJ_SIGN   = 1 << 5,
  };

  using CertInfoList = std::vector<CertInfo>;

  using ListCertsInfoCallback =
      base::OnceCallback<void(CertInfoList certs_info)>;

  using ListCertsCallback =
      base::OnceCallback<void(ScopedCERTCertificateList certs)>;

  using DeleteCertCallback = base::OnceCallback<void(bool)>;

  // Creates a NSSCertDatabase that will store public information (such as
  // certificates and trust records) in |public_slot|, and private information
  // (such as keys) in |private_slot|.
  // In general, code should avoid creating an NSSCertDatabase directly,
  // as doing so requires making opinionated decisions about where to store
  // data, and instead prefer to be passed an existing NSSCertDatabase
  // instance.
  // |public_slot| must not be NULL, |private_slot| can be NULL. Both slots can
  // be identical.
  NSSCertDatabase(crypto::ScopedPK11Slot public_slot,
                  crypto::ScopedPK11Slot private_slot);

  NSSCertDatabase(const NSSCertDatabase&) = delete;
  NSSCertDatabase& operator=(const NSSCertDatabase&) = delete;

  virtual ~NSSCertDatabase();

  // Asynchronously get a list of unique certificates in the certificate
  // database (one instance of all certificates). Note that the callback may be
  // run even after the database is deleted.
  virtual void ListCerts(ListCertsCallback callback);

  // Get a list of certificates in the certificate database of the given slot.
  // Note that the callback may be run even after the database is deleted. Must
  // be called on the IO thread. This does not block by retrieving the certs
  // asynchronously on a worker thread.
  virtual void ListCertsInSlot(ListCertsCallback callback, PK11SlotInfo* slot);

  enum class NSSRootsHandling {
    kInclude,
    kExclude,
  };
  // Asynchronously get a list of certificates along with additional
  // information. Note that the callback may be run even after the database is
  // deleted.
  // The `nss_roots_handling` parameter controls whether to include or exclude
  // NSS built-in roots from the returned list.
  // TODO(crbug.com/40890963): remove the `nss_roots_handling` parameter.
  virtual void ListCertsInfo(ListCertsInfoCallback callback,
                             NSSRootsHandling nss_roots_handling);

#if BUILDFLAG(IS_CHROMEOS)
  // Get the slot for system-wide key data. May be NULL if the system token was
  // not enabled for this database.
  virtual crypto::ScopedPK11Slot GetSystemSlot() const;

  // Checks whether |cert| is stored on |slot|.
  static bool IsCertificateOnSlot(CERTCertificate* cert, PK11SlotInfo* slot);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Get the default slot for public key data.
  crypto::ScopedPK11Slot GetPublicSlot() const;

  // Get the default slot for private key or mixed private/public key data.
  // Can return NULL.
  crypto::ScopedPK11Slot GetPrivateSlot() const;

  // Get all modules.
  // If |need_rw| is true, only writable modules will be returned.
  virtual void ListModules(std::vector<crypto::ScopedPK11Slot>* modules,
                           bool need_rw) const;

  // Set trust values for certificate.
  // Returns true on success or false on failure.
  virtual bool SetCertTrust(CERTCertificate* cert,
                            CertType type,
                            TrustBits trust_bits);

  // Import certificates and private keys from PKCS #12 blob into the module.
  // If |is_extractable| is false, mark the private key as being unextractable
  // from the module.
  // Returns OK or a network error code such as ERR_PKCS12_IMPORT_BAD_PASSWORD
  // or ERR_PKCS12_IMPORT_ERROR. |imported_certs|, if non-NULL, returns a list
  // of certs that were imported.
  int ImportFromPKCS12(PK11SlotInfo* slot_info,
                       const std::string& data,
                       const std::u16string& password,
                       bool is_extractable,
                       ScopedCERTCertificateList* imported_certs);

  // Export the given certificates and private keys into a PKCS #12 blob,
  // storing into |output|.
  // Returns the number of certificates successfully exported. NSS has to be
  // initialized before the method is called.
  static int ExportToPKCS12(const ScopedCERTCertificateList& certs,
                            const std::u16string& password,
                            std::string* output);

  // Uses similar logic to nsNSSCertificateDB::handleCACertDownload to find the
  // root.  Assumes the list is an ordered hierarchy with the root being either
  // the first or last element.
  // TODO(mattm): improve this to handle any order.
  CERTCertificate* FindRootInList(
      const ScopedCERTCertificateList& certificates) const;

  // Import a user certificate. The private key for the user certificate must
  // already be installed, otherwise we return ERR_NO_PRIVATE_KEY_FOR_CERT.
  // Returns OK or a network error code.
  int ImportUserCert(const std::string& data);
  int ImportUserCert(CERTCertificate* cert);

  // Import CA certificates.
  // Tries to import all the certificates given.  The root will be trusted
  // according to |trust_bits|.  Any certificates that could not be imported
  // will be listed in |not_imported|.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportCACerts(const ScopedCERTCertificateList& certificates,
                     TrustBits trust_bits,
                     ImportCertFailureList* not_imported);

  // Import server certificate.  The first cert should be the server cert.  Any
  // additional certs should be intermediate/CA certs and will be imported but
  // not given any trust.
  // Any certificates that could not be imported will be listed in
  // |not_imported|.
  // |trust_bits| can be set to explicitly trust or distrust the certificate, or
  // use TRUST_DEFAULT to inherit trust as normal.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportServerCert(const ScopedCERTCertificateList& certificates,
                        TrustBits trust_bits,
                        ImportCertFailureList* not_imported);

  // Get trust bits for certificate.
  TrustBits GetCertTrust(const CERTCertificate* cert, CertType type) const;

  // Delete certificate and associated private key (if one exists).
  // |cert| is still valid when this function returns. Returns true on
  // success.
  bool DeleteCertAndKey(CERTCertificate* cert);

  // Like DeleteCertAndKey but does not block by running the removal on a worker
  // thread. This must be called on IO thread and it will run |callback| on IO
  // thread. Never calls |callback| synchronously.
  void DeleteCertAndKeyAsync(ScopedCERTCertificate cert,
                             DeleteCertCallback callback);

  // IsUntrusted returns true if |cert| is specifically untrusted. These
  // certificates are stored in the database for the specific purpose of
  // rejecting them.
  // TODO(mattm): that's not actually what this method does. (It also marks
  // certs that are self-issued and don't have any specific trust as untrusted,
  // which is wrong.)
  static bool IsUntrusted(const CERTCertificate* cert);

  // IsWebTrustAnchor returns true if |cert| is explicitly trusted for web
  // navigations according to the trust bits stored in the database.
  static bool IsWebTrustAnchor(const CERTCertificate* cert);

  // Check whether cert is stored in a readonly slot.
  // TODO(mattm): this is ill-defined if the cert exists on both readonly and
  // non-readonly slots.
  static bool IsReadOnly(const CERTCertificate* cert);

  // Check whether cert is stored in a hardware slot.
  // This should only be invoked on a worker thread due to expensive operations
  // behind it.
  static bool IsHardwareBacked(const CERTCertificate* cert);

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  // NOTE: Observers registered here will only receive notifications generated
  // directly through the NSSCertDatabase, but not those from the CertDatabase.
  // CertDatabase observers will receive all certificate notifications.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  void RemoveObserver(Observer* observer);

 protected:
  // Returns a list of certificates extracted from |certs_info| list ignoring
  // additional information.
  static ScopedCERTCertificateList ExtractCertificates(CertInfoList certs_info);

  // Certificate listing implementation used by |ListCerts*|. Static so it may
  // safely be used on the worker thread. If |slot| is nullptr, obtains the
  // certs of all slots, otherwise only of |slot|.
  static ScopedCERTCertificateList ListCertsImpl(crypto::ScopedPK11Slot slot);

  // Implements the logic behind returning a list of certificates along with
  // additional information about every certificate.
  // If |add_certs_info| is false, doesn't compute the certificate additional
  // information, the corresponding CertInfo struct fields will be left on their
  // default values.
  // Static so it may safely be used on the worker thread. If |slot| is nullptr,
  // obtains the certs of all slots, otherwise only of |slot|.
  // The |nss_roots_handling| parameter controls whether to include or exclude
  // NSS built-in roots from the resulting cert list.
  static CertInfoList ListCertsInfoImpl(crypto::ScopedPK11Slot slot,
                                        bool add_certs_info,
                                        NSSRootsHandling nss_roots_handling);

  // Broadcasts notifications to all registered observers.
  void NotifyObserversTrustStoreChanged();
  void NotifyObserversClientCertStoreChanged();

 private:
  enum class DeleteCertAndKeyResult {
    ERROR,
    OK_FOUND_KEY,
    OK_NO_KEY,
  };
  // Notifies observers of the removal of a cert and calls |callback| with
  // |success| as argument.
  void NotifyCertRemovalAndCallBack(DeleteCertCallback callback,
                                    DeleteCertAndKeyResult result);

  // Certificate removal implementation used by |DeleteCertAndKey*|. Static so
  // it may safely be used on the worker thread.
  static DeleteCertAndKeyResult DeleteCertAndKeyImpl(CERTCertificate* cert);
  // Like above, but taking a ScopedCERTCertificate. This is a workaround for
  // base::Bind not having a way to own a unique_ptr but pass it to the
  // function as a raw pointer.
  static DeleteCertAndKeyResult DeleteCertAndKeyImplScoped(
      ScopedCERTCertificate cert);

  crypto::ScopedPK11Slot public_slot_;
  crypto::ScopedPK11Slot private_slot_;

  // A helper observer that forwards events from this database to CertDatabase.
  std::unique_ptr<Observer> cert_notification_forwarder_;

  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;

  base::WeakPtrFactory<NSSCertDatabase> weak_factory_{this};
};

}  // namespace net

#endif  // NET_CERT_NSS_CERT_DATABASE_H_
