// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTACTS_PICKER_PROPERTIES_H_
#define CONTENT_PUBLIC_BROWSER_CONTACTS_PICKER_PROPERTIES_H_

namespace content {

// These values are written to logs as bitmasks (combination of names/emails
// and/or telephones). New enum values can be added, but existing enums must
// never be renumbered or deleted and reused. A Java counterpart will be
// generated from this enum.
// GENERATED_JAVA_ENUM_PACKAGE:org.chromium.content.browser.contacts
enum ContactsPickerProperties {
  PROPERTIES_NONE = 0,
  PROPERTIES_TELS = 1 << 0,
  PROPERTIES_EMAILS = 1 << 1,
  PROPERTIES_NAMES = 1 << 2,
  PROPERTIES_ADDRESSES = 1 << 3,
  PROPERTIES_ICONS = 1 << 4,
  PROPERTIES_BOUNDARY = 1 << 5,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTACTS_PICKER_PROPERTIES_H_
