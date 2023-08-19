// Copyright 2012 The Chromium Authors
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
#include "rlz/lib/supplementary_branding.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

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

// The maximum length of an access points RLZ in bytes.
const size_t kMaxRlzLength = 64;
// The maximum length of a CGI string in bytes.
const size_t kMaxCgiLength = 2048;

// Set the URLLoaderFactory used by SendFinancialPing().
bool RLZ_LIB_API SetURLLoaderFactory(network::mojom::URLLoaderFactory* factory);

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
// SetURLLoaderFactory() needs to be called before calling this function.
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
// SendFinancialPing() will retry (best effort) the request on failures.
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

}  // namespace rlz_lib

#endif  // RLZ_LIB_RLZ_LIB_H_
