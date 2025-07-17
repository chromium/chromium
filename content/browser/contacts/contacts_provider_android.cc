// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_provider_android.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/url_formatter/elide_url.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/contacts_picker_properties.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ContactsDialogHost_jni.h"

namespace content {

ContactsProviderAndroid::ContactsProviderAndroid(
    RenderFrameHostImpl* render_frame_host) {
  JNIEnv* env = base::android::AttachCurrentThread();

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents || !web_contents->GetTopLevelNativeWindow())
    return;

  formatted_origin_ = url_formatter::FormatUrlForSecurityDisplay(
      render_frame_host->GetLastCommittedOrigin().GetURL(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  dialog_.Reset(
      Java_ContactsDialogHost_create(env, web_contents->GetJavaWebContents(),
                                     reinterpret_cast<intptr_t>(this)));
  DCHECK(!dialog_.is_null());
}

ContactsProviderAndroid::~ContactsProviderAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_destroy(env, dialog_);
}

void ContactsProviderAndroid::Select(bool multiple,
                                     bool include_names,
                                     bool include_emails,
                                     bool include_tel,
                                     bool include_addresses,
                                     bool include_icons,
                                     ContactsSelectedCallback callback) {
  if (!dialog_) {
    std::move(callback).Run(std::nullopt, /*percentage_shared=*/-1,
                            PROPERTIES_NONE);
    return;
  }

  callback_ = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_showDialog(
      env, dialog_, multiple, include_names, include_emails, include_tel,
      include_addresses, include_icons,
      base::android::ConvertUTF16ToJavaString(env, formatted_origin_));
}

void ContactsProviderAndroid::AddContact(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& names_java,
    const base::android::JavaParamRef<jobjectArray>& emails_java,
    const base::android::JavaParamRef<jobjectArray>& tel_java,
    const base::android::JavaParamRef<jobjectArray>& addresses_java,
    const base::android::JavaParamRef<jobjectArray>& icons_java) {
  DCHECK(callback_);

  std::optional<std::vector<std::string>> names;
  if (names_java) {
    std::vector<std::string> names_vector;
    base::android::AppendJavaStringArrayToStringVector(env, names_java,
                                                       &names_vector);
    names = std::move(names_vector);
  }

  std::optional<std::vector<std::string>> emails;
  if (emails_java) {
    std::vector<std::string> emails_vector;
    base::android::AppendJavaStringArrayToStringVector(env, emails_java,
                                                       &emails_vector);
    emails = std::move(emails_vector);
  }

  std::optional<std::vector<std::string>> tel;
  if (tel_java) {
    std::vector<std::string> tel_vector;
    base::android::AppendJavaStringArrayToStringVector(env, tel_java,
                                                       &tel_vector);
    tel = std::move(tel_vector);
  }

  std::optional<std::vector<payments::mojom::PaymentAddressPtr>> addresses;
  if (addresses_java) {
    std::vector<payments::mojom::PaymentAddressPtr> addresses_vector;

    for (const base::android::JavaRef<jbyteArray>& j_address :
         addresses_java.ReadElements<jbyteArray>()) {
      payments::mojom::PaymentAddressPtr address;
      base::span<const uint8_t> address_bytes =
          base::android::JavaByteBufferToSpan(env, j_address.obj());
      if (!payments::mojom::PaymentAddress::Deserialize(
              address_bytes.data(), address_bytes.size(), &address)) {
        continue;
      }
      addresses_vector.push_back(std::move(address));
    }

    addresses = std::move(addresses_vector);
  }

  std::optional<std::vector<blink::mojom::ContactIconBlobPtr>> icons;
  if (icons_java) {
    std::vector<blink::mojom::ContactIconBlobPtr> icons_vector;

    for (const base::android::JavaRef<jbyteArray>& j_icon :
         icons_java.ReadElements<jbyteArray>()) {
      blink::mojom::ContactIconBlobPtr icon;
      base::span<const uint8_t> icon_bytes =
          base::android::JavaByteBufferToSpan(env, j_icon.obj());
      if (!blink::mojom::ContactIconBlob::Deserialize(
              icon_bytes.data(), icon_bytes.size(), &icon)) {
        continue;
      }
      icons_vector.push_back(std::move(icon));
    }

    icons = std::move(icons_vector);
  }

  blink::mojom::ContactInfoPtr contact = blink::mojom::ContactInfo::New(
      std::move(names), std::move(emails), std::move(tel), std::move(addresses),
      std::move(icons));

  contacts_.push_back(std::move(contact));
}

void ContactsProviderAndroid::EndContactsList(JNIEnv* env,
                                              jint percentage_shared,
                                              jint properties_requested) {
  DCHECK(callback_);
  ContactsPickerProperties properties =
      static_cast<ContactsPickerProperties>(properties_requested);
  std::move(callback_).Run(std::move(contacts_), percentage_shared, properties);
}

void ContactsProviderAndroid::EndWithPermissionDenied(JNIEnv* env) {
  DCHECK(callback_);
  std::move(callback_).Run(std::nullopt, /*percentage_shared=*/-1,
                           PROPERTIES_NONE);
}

}  // namespace content
