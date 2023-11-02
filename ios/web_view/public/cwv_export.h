// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_EXPORT_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_EXPORT_H_

// All public classes in //ios/web_view must be prefixed with CWV_EXPORT, so
// that clients of //ios/web_view dynamic library can link them.
//
// This is because a dynamic library only exports symbols marked
// __attribute__((visibility("default"))).
//
// Define a macro CWV_EXPORT instead of directly using __attribute__.
// This makes it possible to export symbols only when *building* the dynamic
// library (by checking CWV_IMPLEMENTATION), not when *using* the dynamic
// library.
#if defined(CWV_IMPLEMENTATION)
#define CWV_EXPORT __attribute__((visibility("default")))
#else
#define CWV_EXPORT
#endif

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_EXPORT_H_
