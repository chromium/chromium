// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SWITCHES_H_
#define IOS_WEB_SWITCHES_H_

namespace web {
namespace switches {

extern const char kDisableAllInjectedScripts[];
extern const char kDisableInjectedFeatureScripts[];
extern const char kDisableListedScripts[];
extern const char kEnableListedScripts[];

extern const char kDisableListedJavascriptFeatures[];
extern const char kEnableListedJavascriptFeatures[];

}  // namespace switches
}  // namespace web

#endif  // IOS_WEB_SWITCHES_H_
