// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_DBUS_EXPORT_H_
#define DBUS_DBUS_EXPORT_H_

// Defines CHROME_DBUS_EXPORT so that functionality implemented by the dbus
// library can be exported to consumers.
// NOTE: We haven't used DBUS_EXPORT because it would conflict with the version
// from /usr/include/dbus-1.0/dbus/dbus-macros.h.

#if defined(WIN32)
#error dbus support is not currently expected to work on windows
#endif  // defined(WIN32)

#if defined(COMPONENT_BUILD)

#if defined(DBUS_IMPLEMENTATION)
#define CHROME_DBUS_EXPORT __attribute__((visibility("default")))
#else
#define CHROME_DBUS_EXPORT
#endif

#else  // !defined(COMPONENT_BUILD)

#define CHROME_DBUS_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // DBUS_DBUS_EXPORT_H_
