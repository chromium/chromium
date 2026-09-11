// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector.h"

#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/network/devtools_durable_msg_collector_manager.h"
#include "third_party/re2/src/re2/re2.h"

namespace network {

DevtoolsDurableMessageCollector::DevtoolsDurableMessageCollector(
    base::WeakPtr<DevtoolsDurableMessageCollectorManager> manager)
    : manager_(manager) {
  manager_->OnCollectorCreated(this);
}

DevtoolsDurableMessageCollector::~DevtoolsDurableMessageCollector() {
  // DevtoolsDurableMessage destructor calls back into this class via
  // WillRemoveBytes(). Explicitly clear the map to avoid a destruction
  // ordering bug.
  request_id_to_message_map_.clear();
  CHECK_EQ(cur_buffer_size_, 0);
  if (manager_) {
    manager_->OnCollectorDestroyed(this);
  }
}

void DevtoolsDurableMessageCollector::Configure(
    mojom::NetworkDurableMessageConfigPtr mojo_config,
    ConfigureCallback callback) {
  max_buffer_size_ =
      std::max(max_buffer_size_,
               static_cast<int64_t>(mojo_config->http_storage_max_size));
  std::move(callback).Run();
}

void DevtoolsDurableMessageCollector::Retrieve(
    const std::string& devtools_request_id,
    RetrieveCallback callback) {
  auto message = request_id_to_message_map_.find(devtools_request_id);
  if (message != request_id_to_message_map_.end() &&
      message->second->is_complete()) {
    return std::move(callback).Run(
        std::make_optional(message->second->Retrieve()));
  }

  return std::move(callback).Run(std::nullopt);
}

namespace {

constexpr size_t kMaxMatches = 1000;

std::optional<std::vector<mojom::MessageSearchMatchPtr>> PerformSearch(
    mojo_base::BigBuffer buffer,
    std::string query_regex,
    bool case_sensitive) {
  std::string_view data_view = base::as_string_view(base::span(buffer));
  if (!base::IsStringUTF8(data_view)) {
    return std::vector<mojom::MessageSearchMatchPtr>{};
  }

  re2::RE2::Options options;
  options.set_case_sensitive(case_sensitive);
  re2::RE2 re(query_regex, options);
  if (!re.ok()) {
    return std::vector<mojom::MessageSearchMatchPtr>{};
  }

  std::vector<mojom::MessageSearchMatchPtr> matches;
  size_t pos = 0;
  int32_t line_number = 0;
  while (pos < data_view.size()) {
    size_t end = data_view.find('\n', pos);
    if (end == std::string_view::npos) {
      end = data_view.size();
    }
    std::string_view line = data_view.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (re2::RE2::PartialMatch(line, re)) {
      auto match = mojom::MessageSearchMatch::New();
      match->line_number = line_number;
      match->line_content = std::string(line);
      matches.push_back(std::move(match));
      if (matches.size() >= kMaxMatches) {
        break;
      }
    }
    pos = end + 1;
    line_number++;
  }
  return matches;
}

}  // namespace

void DevtoolsDurableMessageCollector::Search(
    const std::string& devtools_request_id,
    const std::string& query_regex,
    bool case_sensitive,
    SearchCallback callback) {
  auto message = request_id_to_message_map_.find(devtools_request_id);
  if (message == request_id_to_message_map_.end() ||
      !message->second->is_complete()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  mojo_base::BigBuffer buffer = message->second->Retrieve();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PerformSearch, std::move(buffer), query_regex,
                     case_sensitive),
      std::move(callback));
}

base::WeakPtr<DevtoolsDurableMessage>
DevtoolsDurableMessageCollector::CreateDurableMessage(
    const std::string& devtools_request_id) {
  auto [it, inserted] = request_id_to_message_map_.insert_or_assign(
      devtools_request_id,
      std::make_unique<DevtoolsDurableMessage>(devtools_request_id, *this));
  auto& message = it->second;

  // Mark eviction order.
  message_queue_.push(message->GetWeakPtr());

  if (manager_) {
    manager_->OnCollectorAddedMessage();
  }

  return message->GetWeakPtr();
}

void DevtoolsDurableMessageCollector::WillAddBytes(
    DevtoolsDurableMessage& message,
    int64_t size) {
  if (size < 0 || size > max_buffer_size_) {
    // This body cannot be stored with the current set limits.
    // If the beginning of this body was already stored, evict it and bail.
    EvictMessage(message);
    return;
  }

  // Evict prior items if we're short on storage buffer, locally or globally.
  while (!message_queue_.empty()) {
    bool local_ok = size <= max_buffer_size_ - cur_buffer_size_;
    bool global_ok = manager_ ? manager_->CanAccommodate(size) : true;
    if (local_ok && global_ok) {
      break;
    }

    auto evict_message = message_queue_.front();
    message_queue_.pop();
    if (evict_message) {
      bool is_current_message_evicted = (evict_message.get() == &message);
      EvictMessage(*evict_message);
      if (is_current_message_evicted) {
        // The message being called with is now evicted, and remaining bytes
        // will not be stored.
        return;
      }
    }
  }

  // Final check: if we're STILL out of global space (because our local queue
  // is empty, but other collectors are hogging global space), we drop *this*
  // message.
  bool local_ok = size <= max_buffer_size_ - cur_buffer_size_;
  bool global_ok = manager_ ? manager_->CanAccommodate(size) : true;
  if (!local_ok || !global_ok) {
    EvictMessage(message);
    return;
  }

  cur_buffer_size_ += size;
  if (manager_) {
    manager_->OnCollectorAddedBytes(size);
  }
}

void DevtoolsDurableMessageCollector::WillRemoveBytes(
    DevtoolsDurableMessage& message) {
  int64_t size = message.size();
  cur_buffer_size_ -= size;
  CHECK_GE(cur_buffer_size_, 0);
  if (manager_) {
    manager_->OnCollectorRemovedBytes(size);
  }
}

void DevtoolsDurableMessageCollector::WillDestroyMessage(
    DevtoolsDurableMessage& message) {
  if (manager_) {
    manager_->OnCollectorRemovedMessage();
  }
}

void DevtoolsDurableMessageCollector::EvictMessage(
    const DevtoolsDurableMessage& message) {
  request_id_to_message_map_.erase(message.request_id());
}

void DevtoolsDurableMessageCollector::EnableForProfile(
    const base::UnguessableToken& profile_id,
    EnableForProfileCallback callback) {
  if (manager_) {
    manager_->EnableForProfile(profile_id, *this);
  }
  std::move(callback).Run();
}

void DevtoolsDurableMessageCollector::DisableForProfile(
    const base::UnguessableToken& profile_id,
    DisableForProfileCallback callback) {
  if (manager_) {
    manager_->DisableForProfile(profile_id, *this);
  }
  std::move(callback).Run();
}

}  // namespace network
