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
 *   Javier Delgadillo <javi@netscape.com>
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

#ifndef NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSNSSCERTIFICATEDB_H_
#define NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSNSSCERTIFICATEDB_H_

#include <vector>

#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"

typedef struct CERTCertificateStr CERTCertificate;
typedef struct PK11SlotInfoStr PK11SlotInfo;

namespace mozilla_security_manager {

bool ImportCACerts(PK11SlotInfo* slot,
                   const net::ScopedCERTCertificateList& certificates,
                   CERTCertificate* root,
                   net::NSSCertDatabase::TrustBits trustBits,
                   net::NSSCertDatabase::ImportCertFailureList* not_imported);

bool ImportServerCert(
    PK11SlotInfo* slot,
    const net::ScopedCERTCertificateList& certificates,
    net::NSSCertDatabase::TrustBits trustBits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported);

int ImportUserCert(CERTCertificate* cert,
                   crypto::ScopedPK11Slot preferred_slot);

bool SetCertTrust(CERTCertificate* cert,
                  net::CertType type,
                  net::NSSCertDatabase::TrustBits trustBits);

}  // namespace mozilla_security_manager

#endif  // NET_THIRD_PARTY_MOZILLA_SECURITY_MANAGER_NSNSSCERTIFICATEDB_H_
