// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/content_index/content_index.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/40631965): Move image utility functions to common library.
using content::background_fetch::DeserializeIcon;
using content::background_fetch::SerializeIcon;

namespace content {

namespace {

constexpr char kEntryPrefix[] = "content_index:entry_";
constexpr char kIconPrefix[] = "content_index:icon_";

std::string EntryKey(const std::string& id) {
  return kEntryPrefix + id;
}

std::string IconsKey(const std::string& id) {
  return kIconPrefix + id;
}

std::string CreateSerializedContentEntry(
    const blink::mojom::ContentDescription& description,
    const GURL& launch_url,
    base::Time entry_time,
    bool is_top_level_context) {
  // Convert description.
  proto::ContentDescription description_proto;
  description_proto.set_id(description.id);
  description_proto.set_title(description.title);
  description_proto.set_description(description.description);
  description_proto.set_category(static_cast<int>(description.category));

  for (const auto& icon : description.icons) {
    auto* icon_proto = description_proto.add_icons();
    icon_proto->set_src(icon->src);
    if (icon->sizes)
      icon_proto->set_sizes(*icon->sizes);
    if (icon->type)
      icon_proto->set_type(*icon->type);
  }

  description_proto.set_launch_url(description.launch_url);

  // Create entry.
  proto::ContentEntry entry;
  *entry.mutable_description() = std::move(description_proto);
  entry.set_launch_url(launch_url.spec());
  entry.set_timestamp(entry_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  entry.set_is_top_level_context(is_top_level_context);

  return entry.SerializeAsString();
}

blink::mojom::ContentDescriptionPtr DescriptionFromProto(
    const proto::ContentDescription& description) {
  // Validate.
  if (description.category() <
          static_cast<int>(blink::mojom::ContentCategory::kMinValue) ||
      description.category() >
          static_cast<int>(blink::mojom::ContentCategory::kMaxValue)) {
    return nullptr;
  }

  // Convert.
  auto result = blink::mojom::ContentDescription::New();
  result->id = description.id();
  result->title = description.title();
  result->description = description.description();
  result->category =
      static_cast<blink::mojom::ContentCategory>(description.category());
  for (const auto& icon : description.icons()) {
    auto mojo_icon = blink::mojom::ContentIconDefinition::New();
    mojo_icon->src = icon.src();
    if (icon.has_sizes())
      mojo_icon->sizes = icon.sizes();
    if (icon.has_type())
      mojo_icon->type = icon.type();
    result->icons.push_back(std::move(mojo_icon));
  }

  result->launch_url = description.launch_url();
  return result;
}

std::optional<ContentIndexEntry> EntryFromSerializedProto(
    int64_t service_worker_registration_id,
    const std::string& serialized_proto) {
  proto::ContentEntry entry_proto;
  if (!entry_proto.ParseFromString(serialized_proto))
    return std::nullopt;

  GURL launch_url(entry_proto.launch_url());
  if (!launch_url.is_valid())
    return std::nullopt;

  auto description = DescriptionFromProto(entry_proto.description());
  base::Time registration_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(entry_proto.timestamp()));

  return ContentIndexEntry(service_worker_registration_id,
                           std::move(description), std::move(launch_url),
                           registration_time,
                           entry_proto.is_top_level_context());
}

}  // namespace

ContentIndexDatabase::ContentIndexDatabase(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : provider_(browser_context->GetContentIndexProvider()),
      service_worker_context_(std::move(service_worker_context)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ContentIndexDatabase::~ContentIndexDatabase() = default;

void ContentIndexDatabase::AddEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    bool is_top_level_context,
    blink::mojom::ContentDescriptionPtr description,
    const std::vector<SkBitmap>& icons,
    const GURL& launch_url,
    blink::mojom::ContentIndexService::AddCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (blocked_origins_.count(origin)) {
    // TODO(crbug.com/40631965): Does this need a more specific error?
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    std::move(callback).Run(blink::mojom::ContentIndexError::NO_SERVICE_WORKER);
    return;
  }

  if (!service_worker_registration->key().origin().IsSameOriginWith(origin)) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  auto serialized_icons = std::make_unique<proto::SerializedIcons>();
  proto::SerializedIcons* serialized_icons_ptr = serialized_icons.get();

  auto barrier_closure = base::BarrierClosure(
      icons.size(),
      base::BindOnce(&ContentIndexDatabase::DidSerializeIcons,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, origin,
                     is_top_level_context, std::move(description), launch_url,
                     std::move(serialized_icons), std::move(callback)));

  for (const auto& icon : icons) {
    SerializeIcon(icon,
                  base::BindOnce(
                      [](base::OnceClosure done_closure,
                         proto::SerializedIcons* icons, std::string icon) {
                        icons->add_icons()->set_icon(std::move(icon));
                        std::move(done_closure).Run();
                      },
                      barrier_closure, serialized_icons_ptr));
  }
}

void ContentIndexDatabase::DidSerializeIcons(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    bool is_top_level_context,
    blink::mojom::ContentDescriptionPtr description,
    const GURL& launch_url,
    std::unique_ptr<proto::SerializedIcons> serialized_icons,
    blink::mojom::ContentIndexService::AddCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time entry_time = base::Time::Now();
  std::string entry_key = EntryKey(description->id);
  std::string icon_key = IconsKey(description->id);
  std::string entry_value = CreateSerializedContentEntry(
      *description, launch_url, entry_time, is_top_level_context);
  std::string icons_value = serialized_icons->SerializeAsString();

  // Entry to pass over to the provider.
  ContentIndexEntry entry(service_worker_registration_id,
                          std::move(description), launch_url, entry_time,
                          is_top_level_context);

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id,
      blink::StorageKey::CreateFirstParty(origin),
      {{std::move(entry_key), std::move(entry_value)},
       {std::move(icon_key), std::move(icons_value)}},
      base::BindOnce(&ContentIndexDatabase::DidAddEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(entry)));
}

void ContentIndexDatabase::DidAddEntry(
    blink::mojom::ContentIndexService::AddCallback callback,
    ContentIndexEntry entry,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);

  std::vector<ContentIndexEntry> entries;
  entries.push_back(std::move(entry));
  NotifyProviderContentAdded(std::move(entries));
}

void ContentIndexDatabase::DeleteEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeleteEntryImpl(service_worker_registration_id, origin, entry_id,
                  std::move(callback));
}

void ContentIndexDatabase::DeleteEntryImpl(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->key().origin().IsSameOriginWith(origin)) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  service_worker_context_->ClearRegistrationUserData(
      service_worker_registration_id, {EntryKey(entry_id), IconsKey(entry_id)},
      base::BindOnce(&ContentIndexDatabase::DidDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, origin, entry_id,
                     std::move(callback)));
}

void ContentIndexDatabase::DidDeleteEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);

  NotifyProviderContentDeleted(service_worker_registration_id, origin,
                               entry_id);
}

void ContentIndexDatabase::GetDescriptions(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::ContentIndexService::GetDescriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->key().origin().IsSameOriginWith(origin)) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                            /* descriptions= */ {});
    return;
  }

  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      service_worker_registration_id, kEntryPrefix,
      base::BindOnce(&ContentIndexDatabase::DidGetDescriptions,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, std::move(callback)));
}

void ContentIndexDatabase::DidGetDescriptions(
    int64_t service_worker_registration_id,
    blink::mojom::ContentIndexService::GetDescriptionsCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                            /* descriptions= */ {});
    return;
  } else if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                            /* descriptions= */ {});
    return;
  }

  std::vector<blink::mojom::ContentDescriptionPtr> descriptions;
  descriptions.reserve(data.size());

  for (const auto& serialized_entry : data) {
    proto::ContentEntry entry;
    if (!entry.ParseFromString(serialized_entry)) {
      ClearServiceWorkerDataOnCorruption(service_worker_registration_id);
      std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                              /* descriptions= */ {});
      return;
    }

    auto description = DescriptionFromProto(entry.description());
    if (!description) {
      // Clear entry data.
      service_worker_context_->ClearRegistrationUserData(
          service_worker_registration_id,
          {EntryKey(entry.description().id()),
           IconsKey(entry.description().id())},
          base::DoNothing());
      continue;
    }

    descriptions.push_back(std::move(description));
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                          std::move(descriptions));
}

void ContentIndexDatabase::GetIcons(
    int64_t service_worker_registration_id,
    const std::string& description_id,
    ContentIndexContext::GetIconsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {IconsKey(description_id)},
      base::BindOnce(&ContentIndexDatabase::DidGetSerializedIcons,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, std::move(callback)));
}

void ContentIndexDatabase::DidGetSerializedIcons(
    int64_t service_worker_registration_id,
    ContentIndexContext::GetIconsCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk || data.empty()) {
    std::move(callback).Run({});
    return;
  }

  DCHECK_EQ(data.size(), 1u);
  proto::SerializedIcons serialized_icons;
  if (!serialized_icons.ParseFromString(data.front())) {
    ClearServiceWorkerDataOnCorruption(service_worker_registration_id);
    std::move(callback).Run({});
    return;
  }

  if (serialized_icons.icons_size() == 0u) {
    // There are no icons.
    std::move(callback).Run({});
    return;
  }

  auto icons = std::make_unique<std::vector<SkBitmap>>();
  std::vector<SkBitmap>* icons_ptr = icons.get();

  auto barrier_closure = base::BarrierClosure(
      serialized_icons.icons_size(),
      base::BindOnce(&ContentIndexDatabase::DidDeserializeIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(icons)));

  for (auto& serialized_icon : *serialized_icons.mutable_icons()) {
    DeserializeIcon(base::WrapUnique(serialized_icon.release_icon()),
                    base::BindOnce(
                        [](base::OnceClosure done_closure,
                           std::vector<SkBitmap>* icons, SkBitmap icon) {
                          icons->push_back(std::move(icon));
                          std::move(done_closure).Run();
                        },
                        barrier_closure, icons_ptr));
  }
}

void ContentIndexDatabase::DidDeserializeIcons(
    ContentIndexContext::GetIconsCallback callback,
    std::unique_ptr<std::vector<SkBitmap>> icons) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(std::move(*icons));
}

void ContentIndexDatabase::GetAllEntries(
    ContentIndexContext::GetAllEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kEntryPrefix,
      base::BindOnce(&ContentIndexDatabase::DidGetEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexDatabase::DidGetEntries(
    ContentIndexContext::GetAllEntriesCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                            /* entries= */ {});
    return;
  }

  if (user_data.empty()) {
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                            /* entries= */ {});
    return;
  }

  std::vector<ContentIndexEntry> entries;
  entries.reserve(user_data.size());
  std::set<int64_t> corrupted_sw_ids;

  for (const auto& ud : user_data) {
    auto entry = EntryFromSerializedProto(ud.first, ud.second);
    if (!entry) {
      corrupted_sw_ids.insert(ud.first);
      continue;
    }

    entries.emplace_back(std::move(*entry));
  }

  if (!corrupted_sw_ids.empty()) {
    // Remove soon-to-be-deleted entries.
    std::erase_if(entries, [&corrupted_sw_ids](const auto& entry) {
      return corrupted_sw_ids.count(entry.service_worker_registration_id);
    });

    for (int64_t service_worker_registration_id : corrupted_sw_ids)
      ClearServiceWorkerDataOnCorruption(service_worker_registration_id);
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                          std::move(entries));
}

void ContentIndexDatabase::GetEntry(
    int64_t service_worker_registration_id,
    const std::string& description_id,
    ContentIndexContext::GetEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {EntryKey(description_id)},
      base::BindOnce(&ContentIndexDatabase::DidGetEntry,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, std::move(callback)));
}

void ContentIndexDatabase::DidGetEntry(
    int64_t service_worker_registration_id,
    ContentIndexContext::GetEntryCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  DCHECK_EQ(data.size(), 1u);
  std::move(callback).Run(
      EntryFromSerializedProto(service_worker_registration_id, data.front()));
}

void ContentIndexDatabase::ClearServiceWorkerDataOnCorruption(
    int64_t service_worker_registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->ClearRegistrationUserDataByKeyPrefixes(
      service_worker_registration_id, {kEntryPrefix, kIconPrefix},
      base::DoNothing());
}

void ContentIndexDatabase::DeleteItem(int64_t service_worker_registration_id,
                                      const url::Origin& origin,
                                      const std::string& description_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeleteEntryImpl(
      service_worker_registration_id, origin, description_id,
      base::BindOnce(&ContentIndexDatabase::DidDeleteItem,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, origin, description_id));
}

void ContentIndexDatabase::DidDeleteItem(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id,
    blink::mojom::ContentIndexError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != blink::mojom::ContentIndexError::NONE)
    return;

  service_worker_context_->FindReadyRegistrationForId(
      service_worker_registration_id,
      blink::StorageKey::CreateFirstParty(origin),
      base::BindOnce(&ContentIndexDatabase::StartActiveWorkerForDispatch,
                     weak_ptr_factory_.GetWeakPtr(), description_id));
}

void ContentIndexDatabase::StartActiveWorkerForDispatch(
    const std::string& description_id,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;

  ServiceWorkerVersion* service_worker_version = registration->active_version();
  DCHECK(service_worker_version);

  service_worker_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::CONTENT_DELETE,
      base::BindOnce(&ContentIndexDatabase::DeliverMessageToWorker,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::WrapRefCounted(service_worker_version),
                     std::move(registration), description_id));
}

void ContentIndexDatabase::DeliverMessageToWorker(
    scoped_refptr<ServiceWorkerVersion> service_worker,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const std::string& description_id,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;

  // Don't allow DB operations while the `contentdelete` event is firing.
  // This is to prevent re-registering the deleted content within the event.
  BlockOrigin(service_worker->key().origin());

  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::CONTENT_DELETE,
      base::BindOnce(&ContentIndexDatabase::DidDispatchEvent,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker->key().origin()));

  service_worker->endpoint()->DispatchContentDeleteEvent(
      description_id, service_worker->CreateSimpleEventCallback(request_id));
}

void ContentIndexDatabase::DidDispatchEvent(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UnblockOrigin(origin);
}

void ContentIndexDatabase::BlockOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocked_origins_[origin]++;
}

void ContentIndexDatabase::UnblockOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = blocked_origins_.find(origin);
  CHECK(it != blocked_origins_.end());
  if (it->second == 1) {
    blocked_origins_.erase(it);
  } else {
    it->second--;
  }
}

void ContentIndexDatabase::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  provider_ = nullptr;
}

void ContentIndexDatabase::NotifyProviderContentAdded(
    std::vector<ContentIndexEntry> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!provider_)
    return;

  for (auto& entry : entries)
    provider_->OnContentAdded(std::move(entry));
}

void ContentIndexDatabase::NotifyProviderContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!provider_)
    return;

  provider_->OnContentDeleted(service_worker_registration_id, origin, entry_id);
}

}  // namespace content
