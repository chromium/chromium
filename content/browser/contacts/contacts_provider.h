// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_H_

#include "content/public/browser/contacts_picker_properties_requested.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

class ContactsProvider {
 public:
  using ContactsSelectedCallback = base::OnceCallback<void(
      base::Optional<std::vector<blink::mojom::ContactInfoPtr>> contacts,
      int percentage_shared,
      ContactsPickerPropertiesRequested properties_requested)>;

  ContactsProvider() = default;
  virtual ~ContactsProvider() = default;

  // Launches the Contacts Picker Dialog and waits for the results to come back.
  // |callback| is called with the contacts list and share statistics, once the
  // operation finishes. See above for details.
  virtual void Select(bool multiple,
                      bool include_names,
                      bool include_emails,
                      bool include_tel,
                      bool include_addresses,
                      bool include_icons,
                      ContactsSelectedCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
