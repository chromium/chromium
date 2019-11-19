// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "content/browser/contacts/contacts_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

class RenderFrameHostImpl;

class ContactsProviderAndroid : public ContactsProvider {
 public:
  explicit ContactsProviderAndroid(RenderFrameHostImpl* render_frame_host);
  ~ContactsProviderAndroid() override;

  // ContactsProvider:
  void Select(bool multiple,
              bool include_names,
              bool include_emails,
              bool include_tel,
              bool include_addresses,
              bool include_icons,
              ContactsSelectedCallback callback) override;

  // Adds one contact to the list of contacts selected. Note, EndContactsList
  // (or EndWithPermissionDenied) must be called to signal the end of the
  // construction of the contacts list.
  void AddContact(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& names_java,
      const base::android::JavaParamRef<jobjectArray>& emails_java,
      const base::android::JavaParamRef<jobjectArray>& tel_java,
      const base::android::JavaParamRef<jobjectArray>& addresses_java,
      const base::android::JavaParamRef<jobjectArray>& icons_java);

  // Signals the end of adding contacts to the list. The contact list is
  // returned to the web page, the other params are logged via UKM.
  void EndContactsList(JNIEnv* env,
                       jint percentage_shared,
                       jint properties_requested);

  // Signals the end (due to a permission error).
  void EndWithPermissionDenied(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> dialog_;

  ContactsSelectedCallback callback_;

  // The list of contacts to return.
  std::vector<blink::mojom::ContactInfoPtr> contacts_;

  // The origin that the contacts data will be shared with. Formatted for
  // display with the scheme omitted.
  base::string16 formatted_origin_;

  DISALLOW_COPY_AND_ASSIGN(ContactsProviderAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
