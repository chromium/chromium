// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A library to manage RLZ information for access-points shared
// across different client applications.
//
// All functions return true on success and false on error.
// This implemenation is thread safe.


#ifndef RLZ_LIB_RLZ_LIB_H_
#define RLZ_LIB_RLZ_LIB_H_

#include <stddef.h>
#include <stdio.h>
#include <string>

#include "build/build_config.h"

#include "rlz/lib/rlz_api.h"
#include "rlz/lib/rlz_enums.h"

// Define one of
// + RLZ_NETWORK_IMPLEMENTATION_WIN_INET: Uses win inet to send financial pings.
// + RLZ_NETWORK_IMPLEMENTATION_CHROME_NET: Uses chrome's network stack to send
//   financial pings. rlz_lib::SetURLLoaderFactory() must be called before
//   any calls to SendFinancialPing().
#if defined(RLZ_NETWORK_IMPLEMENTATION_WIN_INET) && \
    defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
#error Exactly one of RLZ_NETWORK_IMPLEMENTATION_WIN_INET and \
    RLZ_NETWORK_IMPLEMENTATION_CHROME_NET should be defined.
#endif
#if !defined(RLZ_NETWORK_IMPLEMENTATION_WIN_INET) && \
    !defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
#if defined(OS_WIN)
#define RLZ_NETWORK_IMPLEMENTATION_WIN_INET
#else
#define RLZ_NETWORK_IMPLEMENTATION_CHROME_NET
#endif
#endif

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network
#endif

namespace rlz_lib {

// All functions return true on success and false on error.
// This implemenation is thread safe.
//
// Each prototype mentions the registry access requirements:
//
// HKLM read:  Will work from any process and at any privilege level on Vista.
// HKCU read:  Calls made from the SYSTEM account must pass the current user's
//             SID as the optional 'sid' param. Can be called from low integrity
//             process on Vista.
// HKCU write: Calls made from the SYSTEM account must pass the current user's
//             SID as the optional 'sid' param. Calls require at least medium
//             integrity on Vista (e.g. Toolbar will need to use their broker)
// HKLM write: Calls must be made from an account with admin rights. No SID
//             need be passed when running as SYSTEM.
// Functions which do not access registry will be marked with "no restrictions".

class ScopedRlzValueStoreLock;

// The maximum length of an access points RLZ in bytes.
const size_t kMaxRlzLength = 64;
// The maximum length of an access points RLZ in bytes.
const size_t kMaxDccLength = 128;
// The maximum length of a CGI string in bytes.
const size_t kMaxCgiLength = 2048;

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
// Set the URLLoaderFactory used by SendFinancialPing().
bool RLZ_LIB_API SetURLLoaderFactory(network::mojom::URLLoaderFactory* factory);
#endif

// RLZ storage functions.

// Get all the events reported by this product as a CGI string to append to
// the daily ping.
// Access: HKCU read.
bool RLZ_LIB_API GetProductEventsAsCgi(Product product, char* unescaped_cgi,
                                       size_t unescaped_cgi_size);

// Records an RLZ event.
// Some events can be product-independent (e.g: First search from home page),
// and some can be access point independent (e.g. Pack installed). However,
// product independent events must still include the product which cares about
// that information being reported.
// Access: HKCU write.
bool RLZ_LIB_API RecordProductEvent(Product product, AccessPoint point,
                                    Event event_id);

// Clear an event reported by this product. This should be called after a
// successful ping to the RLZ server.
// Access: HKCU write.
bool RLZ_LIB_API ClearProductEvent(Product product, AccessPoint point,
                                   Event event_id);

// Clear all reported events and recorded stateful events of this product.
// This should be called on complete uninstallation of the product.
// Access: HKCU write.
bool RLZ_LIB_API ClearAllProductEvents(Product product);

// Clears all product-specifc state from the RLZ registry.
// Should be called during product uninstallation.
// This removes outstanding product events, product financial ping times,
// the product RLS argument (if any), and any RLZ's for access points being
// uninstalled with the product.
// access_points is an array terminated with NO_ACCESS_POINT.
// IMPORTANT: These are the access_points the product is removing as part
// of the uninstallation, not necessarily all the access points passed to
// SendFinancialPing() and GetPingParams().
// access_points can be NULL if no points are being uninstalled.
// No return value - this is best effort. Will assert in debug mode on
// failed attempts.
// Access: HKCU write.
void RLZ_LIB_API ClearProductState(Product product,
                                   const AccessPoint* access_points);

// Get the RLZ value of the access point. If the access point is not Google, the
// RLZ will be the empty string and the function will return false.
// Access: HKCU read.
bool RLZ_LIB_API GetAccessPointRlz(AccessPoint point, char* rlz,
                                   size_t rlz_size);

// Set the RLZ for the access-point. Fails and asserts if called when the access
// point is not set to Google.
// new_rlz should come from a server-response. Client applications should not
// create their own RLZ values.
// Access: HKCU write.
bool RLZ_LIB_API SetAccessPointRlz(AccessPoint point, const char* new_rlz);

// Use |brand| to replace the brand code contained in existing access point RLZ
// strings found in the RLZ data file. Return true if at least one access point
// RLZ string is updated, otherwise return false (and the function is a no-op).
// See https://crbug.com/846033.
// Access: HKCU write.
bool RLZ_LIB_API UpdateExistingAccessPointRlz(const std::string& brand);

// Financial Server pinging functions.
// These functions deal with pinging the RLZ financial server and parsing and
// acting upon the response. Clients should SendFinancialPing() to avoid needing
// these functions. However, these functions allow clients to split the various
// parts of the pinging process up as needed (to avoid firewalls, etc).

// Forms the HTTP request to send to the RLZ financial server.
//
// product            : The product to ping for.
// access_points      : The access points this product affects. Array must be
//                      terminated with NO_ACCESS_POINT.
// product_signature  : The signature sent with daily pings (e.g. swg, ietb)
// product_brand      : The brand of the pinging product, if any.
// product_id         : The product-specific installation ID (can be NULL).
// product_lang       : The language for the product (used to determine cohort).
// exclude_machine_id : Whether the Machine ID should be explicitly excluded
//                      based on the products privacy policy.
// request            : The buffer where the function returns the HTTP request.
// request_buffer_size: The size of the request buffer in bytes. The buffer
//                      size (kMaxCgiLength+1) is guaranteed to be enough.
//
// Access: HKCU read.
bool RLZ_LIB_API FormFinancialPingRequest(Product product,
                                          const AccessPoint* access_points,
                                          const char* product_signature,
                                          const char* product_brand,
                                          const char* product_id,
                                          const char* product_lang,
                                          bool exclude_machine_id,
                                          char* request,
                                          size_t request_buffer_size);

// Complex helpers built on top of other functions.

// Parses the responses from the financial server and updates product state
// and access point RLZ's in registry. Like ParsePingResponse(), but also
// updates the last ping time.
// Access: HKCU write.
bool RLZ_LIB_API ParseFinancialPingResponse(Product product,
                                            const char* response);

// Send the ping with RLZs and events to the PSO server.
// This ping method should be called daily. (More frequent calls will fail).
// Also, if there are no events, the call will succeed only once a week.
//
// If RLZ_NETWORK_IMPLEMENTATION_CHROME_NET is set, SetURLLoaderFactory() needs
// to be called before calling this function.
//
// product            : The product to ping for.
// access_points      : The access points this product affects. Array must be
//                      terminated with NO_ACCESS_POINT.
// product_signature  : The signature sent with daily pings (e.g. swg, ietb)
// product_brand      : The brand of the pinging product, if any.
// product_id         : The product-specific installation ID (can be NULL).
// product_lang       : The language for the product (used to determine cohort).
// exclude_machine_id : Whether the Machine ID should be explicitly excluded
//                      based on the products privacy policy.
//
// Returns true on successful ping and response, false otherwise.
// Access: HKCU write.
bool RLZ_LIB_API SendFinancialPing(Product product,
                                   const AccessPoint* access_points,
                                   const char* product_signature,
                                   const char* product_brand,
                                   const char* product_id,
                                   const char* product_lang,
                                   bool exclude_machine_id);

// An alternate implementations of SendFinancialPing with the same behavior,
// except the caller can optionally choose to skip the timing check.
bool RLZ_LIB_API SendFinancialPing(Product product,
                                   const AccessPoint* access_points,
                                   const char* product_signature,
                                   const char* product_brand,
                                   const char* product_id,
                                   const char* product_lang,
                                   bool exclude_machine_id,
                                   const bool skip_time_check);

// Parses RLZ related ping response information from the server.
// Updates stored RLZ values and clears stored events accordingly.
// Access: HKCU write.
bool RLZ_LIB_API ParsePingResponse(Product product, const char* response);


// Copies the events associated with the product and the RLZ's for each access
// point in access_points into cgi. This string can be directly appended
// to a ping (will need an & if not first paramter).
// access_points must be an array of AccessPoints terminated with
// NO_ACCESS_POINT.
// Access: HKCU read.
bool RLZ_LIB_API GetPingParams(Product product,
                               const AccessPoint* access_points,
                               char* unescaped_cgi, size_t unescaped_cgi_size);

#if defined(OS_WIN)
// OEM Deal confirmation storage functions. OEM Deals are windows-only.

// Makes the OEM Deal Confirmation code writable by all users on the machine.
// This should be called before calling SetMachineDealCode from a non-admin
// account.
// Access: HKLM write.
bool RLZ_LIB_API CreateMachineState(void);

// Set the OEM Deal Confirmation Code (DCC). This information is used for RLZ
// initalization.
// Access: HKLM write, or
// HKCU read if rlz_lib::CreateMachineState() has been successfully called.
bool RLZ_LIB_API SetMachineDealCode(const char* dcc);

// Get the DCC cgi argument string to append to a daily ping.
// Should be used only by OEM deal trackers. Applications should use the
// GetMachineDealCode method which has an AccessPoint paramter.
// Access: HKLM read.
bool RLZ_LIB_API GetMachineDealCodeAsCgi(char* cgi, size_t cgi_size);

// Get the DCC value stored in registry.
// Should be used only by OEM deal trackers. Applications should use the
// GetMachineDealCode method which has an AccessPoint paramter.
// Access: HKLM read.
bool RLZ_LIB_API GetMachineDealCode(char* dcc, size_t dcc_size);

// Parses a ping response, checks if it is valid and sets the machine DCC
// from the response. The ping must also contain the current DCC value in
// order to be considered valid.
// Access: HKLM write;
//         HKCU write if CreateMachineState() has been successfully called.
bool RLZ_LIB_API SetMachineDealCodeFromPingResponse(const char* response);

#endif

// Segment RLZ persistence based on branding information.
// All information for a given product is persisted under keys with the either
// product's name or its access point's name.  This assumes that only
// one instance of the product is installed on the machine, and that only one
// product brand is associated with it.
//
// In some cases, a given product may be using supplementary brands.  The RLZ
// information must be kept separately for each of these brands.  To achieve
// this segmentation, scope all RLZ library calls that deal with supplementary
// brands within the lifetime of an rlz_lib::ProductBranding instance.
//
// For example, to record events for a supplementary brand, do the following:
//
//  {
//    rlz_lib::SupplementaryBranding branding("AAAA");
//    // This call to RecordProductEvent is scoped to the AAAA brand.
//    rlz_lib::RecordProductEvent(rlz_lib::DESKTOP, rlz_lib::GD_DESKBAND,
//                                rlz_lib::INSTALL);
//  }
//
//  // This call to RecordProductEvent is not scoped to any supplementary brand.
//  rlz_lib::RecordProductEvent(rlz_lib::DESKTOP, rlz_lib::GD_DESKBAND,
//                              rlz_lib::INSTALL);
//
// In particular, this affects the recording of stateful events and the sending
// of financial pings.  In the former case, a stateful event recorded while
// scoped to a supplementary brand will be recorded again when scoped to a
// different supplementary brand (or not scoped at all).  In the latter case,
// the time skip check is specific to each supplementary brand.
class SupplementaryBranding {
 public:
  explicit SupplementaryBranding(const char* brand);
  ~SupplementaryBranding();

  static const std::string& GetBrand();

 private:
  ScopedRlzValueStoreLock* lock_;
};

}  // namespace rlz_lib

#endif  // RLZ_LIB_RLZ_LIB_H_
