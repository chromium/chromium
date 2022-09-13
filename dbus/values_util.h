// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_VALUES_UTIL_H_
#define DBUS_VALUES_UTIL_H_

#include <stdint.h>

#include "dbus/dbus_export.h"

namespace base {
class Value;
class ValueView;
}

namespace dbus {

class MessageReader;
class MessageWriter;

// Pops a value from |reader| as a base::Value.
// Returns base::Value() if an error occurs.
// Note: Integer values larger than int32_t (including uint32_t) are converted
// to double.  Non-string dictionary keys are converted to strings.
CHROME_DBUS_EXPORT base::Value PopDataAsValue(MessageReader* reader);

// Appends a basic type value to |writer|. Basic types are BOOLEAN, INTEGER,
// DOUBLE, and STRING. Use this function for values that are known to be basic
// types and to handle basic type members of collections that should not
// have type "a{sv}" or "av". Otherwise, use AppendValueData.
CHROME_DBUS_EXPORT void AppendBasicTypeValueData(MessageWriter* writer,
                                                 base::ValueView value);

// Appends a basic type value to |writer| as a variant. Basic types are BOOLEAN,
// INTEGER, DOUBLE, and STRING. Use this function for values that are known to
// be basic types and to handle basic type members of collections that should
// not have type "a{sv}" or "av". Otherwise, use AppendValueDataAsVariant.
CHROME_DBUS_EXPORT void AppendBasicTypeValueDataAsVariant(
    MessageWriter* writer,
    base::ValueView value);

// Appends a value to |writer|. Value can be a basic type, as well as a
// collection type, such as dictionary or list. Collections will be recursively
// written as variant containers, i.e. dictionaries will be written with type
// a{sv} and lists with type av. Any sub-dictionaries or sub-lists will also
// have these types.
CHROME_DBUS_EXPORT void AppendValueData(MessageWriter* writer,
                                        base::ValueView value);

// Appends a value to |writer| as a variant. Value can be a basic type, as well
// as a collection type, such as dictionary or list. Collections will be
// recursively written as variant containers, i.e. dictionaries will be written
// with type a{sv} and lists with type av. Any sub-dictionaries or sub-lists
// will also have these types.
CHROME_DBUS_EXPORT void AppendValueDataAsVariant(MessageWriter* writer,
                                                 base::ValueView value);

}  // namespace dbus

#endif  // DBUS_VALUES_UTIL_H_
