// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Key and value names of the location of the RLZ shared state.

#ifndef RLZ_LIB_LIB_VALUES_H_
#define RLZ_LIB_LIB_VALUES_H_

#include <stdint.h>

#include "rlz/lib/rlz_enums.h"

namespace rlz_lib {

//
// Ping CGI arguments:
//
//   Events are reported as (without spaces):
//   kEventsCgiVariable = <AccessPoint1><Event1> kEventsCgiSeparator <P2><E2>...
//
//   Event responses from the server look like:
//   kEventsCgiVariable : <AccessPoint1><Event1> kEventsCgiSeparator <P2><E2>...
//
//   RLZ's are reported as (without spaces):
//   kRlzCgiVariable = <AccessPoint> <kRlzCgiIndicator> <RLZ value>
//        <kRlzCgiSeparator> <AP2><Indicator><V2><Separator> ....
//
//   RLZ responses from the server look like (without spaces):
//   kRlzCgiVariable<Access Point> :  <RLZ value>
//
//   DCC if reported should look like (without spaces):
//   kDccCgiVariable = <DCC Value>
//
//   RLS if reported should look like (without spaces):
//   kRlsCgiVariable = <RLS Value>
//
//   Machine ID if reported should look like (without spaces):
//   kMachineIdCgiVariable = <Machine ID Value>
//
//   A server response setting / confirming the DCC will look like (no spaces):
//   kDccCgiVariable : <DCC Value>
//
//   Each ping to the server must also contain kProtocolCgiArgument as well.
//
//   Pings may also contain (but not necessarily controlled by this Lib):
//   - The product signature: kProductSignatureCgiVariable = <signature>
//   - The product brand: kProductBrandCgiVariable = <brand>
//   - The product installation ID: kProductIdCgiVariable = <id>
extern const char kEventsCgiVariable[];
extern const char kStatefulEventsCgiVariable[];
extern const char kEventsCgiSeparator;

extern const char kDccCgiVariable[];
extern const char kProtocolCgiArgument[];

extern const char kProductSignatureCgiVariable[];
extern const char kProductBrandCgiVariable[];
extern const char kProductLanguageCgiVariable[];
extern const char kProductIdCgiVariable[];

extern const char kRlzCgiVariable[];
extern const char kRlzCgiSeparator[];
extern const char kRlzCgiIndicator[];

extern const char kRlsCgiVariable[];
extern const char kMachineIdCgiVariable[];
extern const char kSetDccResponseVariable[];

//
// Financial ping server information.
//

extern const char kFinancialPingPath[];
extern const char kFinancialServer[];

extern const int kFinancialPort;

extern const int64_t kEventsPingInterval;
extern const int64_t kNoEventsPingInterval;

extern const char kFinancialPingUserAgent[];
extern const char* kFinancialPingResponseObjects[];

//
// The names for AccessPoints and Events that we use MUST be the same
// as those used/understood by the server.
//
const char* GetAccessPointName(AccessPoint point);
bool GetAccessPointFromName(const char* name, AccessPoint* point);

const char* GetEventName(Event event);
bool GetEventFromName(const char* name, Event* event);

// The names for products are used only client-side.
const char* GetProductName(Product product);

}  // namespace rlz_lib

#endif  // RLZ_LIB_LIB_VALUES_H_
