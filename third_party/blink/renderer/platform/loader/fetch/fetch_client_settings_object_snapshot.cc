// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"

namespace blink {

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const FetchClientSettingsObject& fetch_client_setting_object)
    : FetchClientSettingsObjectSnapshot(
          fetch_client_setting_object.GlobalObjectUrl(),
          fetch_client_setting_object.BaseUrl(),
          fetch_client_setting_object.GetSecurityOrigin(),
          fetch_client_setting_object.GetReferrerPolicy(),
          fetch_client_setting_object.GetOutgoingReferrer(),
          fetch_client_setting_object.GetHttpsState(),
          fetch_client_setting_object.MimeTypeCheckForClassicWorkerScript(),
          fetch_client_setting_object.GetInsecureRequestsPolicy(),
          fetch_client_setting_object.GetUpgradeInsecureNavigationsSet()) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData> data)
    : FetchClientSettingsObjectSnapshot(
          data->global_object_url,
          data->base_url,
          data->security_origin,
          data->referrer_policy,
          data->outgoing_referrer,
          data->https_state,
          data->mime_type_check_for_classic_worker_script,
          data->insecure_requests_policy,
          data->insecure_navigations_set) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const KURL& global_object_url,
    const KURL& base_url,
    const scoped_refptr<const SecurityOrigin> security_origin,
    network::mojom::ReferrerPolicy referrer_policy,
    const String& outgoing_referrer,
    HttpsState https_state,
    AllowedByNosniff::MimeTypeCheck mime_type_check_for_classic_worker_script,
    mojom::blink::InsecureRequestPolicy insecure_requests_policy,
    InsecureNavigationsSet insecure_navigations_set)
    : global_object_url_(global_object_url),
      base_url_(base_url),
      security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy),
      outgoing_referrer_(outgoing_referrer),
      https_state_(https_state),
      mime_type_check_for_classic_worker_script_(
          mime_type_check_for_classic_worker_script),
      insecure_requests_policy_(insecure_requests_policy),
      insecure_navigations_set_(std::move(insecure_navigations_set)) {}

}  // namespace blink
