// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_FORM_WARNING_TYPE_H_
#define IOS_WEB_PUBLIC_NAVIGATION_FORM_WARNING_TYPE_H_

namespace web {

// Used to specify the type of the form warning that should be shown to the
// user.
enum class FormWarningType {
  kNone = 0,

  // Navigation was a form submission that's being reloaded.
  kRepost,

  // Navigation was an insecure form submission (e.g. from an HTTPS page to an
  // HTTP page).
  kInsecureForm,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_FORM_WARNING_TYPE_H_
