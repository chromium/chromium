// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_EXPORT_H_
#define SANDBOX_MAC_SEATBELT_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(SEATBELT_IMPLEMENTATION)
#define SEATBELT_EXPORT __attribute__((visibility("default")))
#else
#define SEATBELT_EXPORT
#endif  // defined(SEATBELT_IMPLEMENTATION)

#else  // defined(COMPONENT_BUILD)

#define SEATBELT_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // SANDBOX_MAC_SEATBELT_EXPORT_H_
