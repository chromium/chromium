// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_

#include <string>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/contacts/contacts_provider.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"
#include "third_party/jni_zero/system_jni_unchecked_exceptions/ByteBuffer_shared_jni.h"

namespace content {

class RenderFrameHost;

class ContactsProviderAndroid : public ContactsProvider {
 public:
  explicit ContactsProviderAndroid(RenderFrameHost* render_frame_host);

  ContactsProviderAndroid(const ContactsProviderAndroid&) = delete;
  ContactsProviderAndroid& operator=(const ContactsProviderAndroid&) = delete;

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
      const base::android::JavaRef<JArray<jstring>>& names_java,
      const base::android::JavaRef<JArray<jstring>>& emails_java,
      const base::android::JavaRef<JArray<jstring>>& tel_java,
      const base::android::JavaRef<JArray<JByteBuffer>>& addresses_java,
      const base::android::JavaRef<JArray<JByteBuffer>>& icons_java);

  // Signals the end of adding contacts to the list. The contact list is
  // returned to the web page, the other params are logged via UKM.
  void EndContactsList(JNIEnv* env,
                       int32_t percentage_shared,
                       int32_t properties_requested);

  // Signals the end (due to a permission error).
  void EndWithPermissionDenied(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> dialog_;

  ContactsSelectedCallback callback_;

  // The list of contacts to return.
  std::vector<blink::mojom::ContactInfoPtr> contacts_;

  // The origin that the contacts data will be shared with. Formatted for
  // display with the scheme omitted.
  std::u16string formatted_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
