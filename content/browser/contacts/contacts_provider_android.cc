// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_provider_android.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/url_formatter/elide_url.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/ContactsDialogHost_jni.h"
#include "content/public/browser/contacts_picker_properties_requested.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

namespace content {

namespace {

void RecordAddressContainsDerivedField(
    const payments::mojom::PaymentAddress& address) {
  if (address.address_line.empty() || address.address_line.front().empty())
    return;

  bool has_derived_field = !address.city.empty() || !address.country.empty() ||
                           !address.postal_code.empty() ||
                           !address.region.empty();
  base::UmaHistogramBoolean("Android.ContactsPicker.AddressHasDerivedField",
                            has_derived_field);
}

}  // namespace

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

  dialog_.Reset(Java_ContactsDialogHost_create(
      env, web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
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
    std::move(callback).Run(base::nullopt, /*percentage_shared=*/-1,
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

  base::Optional<std::vector<std::string>> names;
  if (names_java) {
    std::vector<std::string> names_vector;
    AppendJavaStringArrayToStringVector(env, names_java, &names_vector);
    names = std::move(names_vector);
  }

  base::Optional<std::vector<std::string>> emails;
  if (emails_java) {
    std::vector<std::string> emails_vector;
    AppendJavaStringArrayToStringVector(env, emails_java, &emails_vector);
    emails = std::move(emails_vector);
  }

  base::Optional<std::vector<std::string>> tel;
  if (tel_java) {
    std::vector<std::string> tel_vector;
    AppendJavaStringArrayToStringVector(env, tel_java, &tel_vector);
    tel = std::move(tel_vector);
  }

  base::Optional<std::vector<payments::mojom::PaymentAddressPtr>> addresses;
  if (addresses_java) {
    std::vector<payments::mojom::PaymentAddressPtr> addresses_vector;

    for (const base::android::JavaRef<jbyteArray>& j_address :
         addresses_java.ReadElements<jbyteArray>()) {
      payments::mojom::PaymentAddressPtr address;
      if (!payments::mojom::PaymentAddress::Deserialize(
              static_cast<jbyte*>(env->GetDirectBufferAddress(j_address.obj())),
              env->GetDirectBufferCapacity(j_address.obj()), &address)) {
        continue;
      }
      RecordAddressContainsDerivedField(*address);
      addresses_vector.push_back(std::move(address));
    }

    addresses = std::move(addresses_vector);
  }

  base::Optional<std::vector<blink::mojom::ContactIconBlobPtr>> icons;
  if (icons_java) {
    std::vector<blink::mojom::ContactIconBlobPtr> icons_vector;

    for (const base::android::JavaRef<jbyteArray>& j_icon :
         icons_java.ReadElements<jbyteArray>()) {
      blink::mojom::ContactIconBlobPtr icon;
      if (!blink::mojom::ContactIconBlob::Deserialize(
              static_cast<jbyte*>(env->GetDirectBufferAddress(j_icon.obj())),
              env->GetDirectBufferCapacity(j_icon.obj()), &icon)) {
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
  ContactsPickerPropertiesRequested properties =
      static_cast<ContactsPickerPropertiesRequested>(properties_requested);
  std::move(callback_).Run(std::move(contacts_), percentage_shared, properties);
}

void ContactsProviderAndroid::EndWithPermissionDenied(JNIEnv* env) {
  DCHECK(callback_);
  std::move(callback_).Run(base::nullopt, /*percentage_shared=*/-1,
                           PROPERTIES_NONE);
}

}  // namespace content
