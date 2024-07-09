// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/receiver_set.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class ReceiverSetState::Entry::DispatchFilter : public MessageFilter {
 public:
  explicit DispatchFilter(Entry& entry,
                          std::unique_ptr<MessageFilter> nested_filter)
      : entry_(entry), nested_filter_(std::move(nested_filter)) {}
  DispatchFilter(const DispatchFilter&) = delete;
  DispatchFilter& operator=(const DispatchFilter&) = delete;
  ~DispatchFilter() override = default;

 private:
  // MessageFilter:
  bool WillDispatch(Message* message) override {
    entry_.WillDispatch();
    if (nested_filter_)
      return nested_filter_->WillDispatch(message);
    return true;
  }

  void DidDispatchOrReject(Message* message, bool accepted) override {
    entry_.DidDispatchOrReject();
    if (nested_filter_)
      nested_filter_->DidDispatchOrReject(message, accepted);
  }

  // RAW_PTR_EXCLUSION: Binary size increase.
  RAW_PTR_EXCLUSION Entry& entry_;
  std::unique_ptr<MessageFilter> nested_filter_;
};

ReceiverSetState::Entry::Entry(ReceiverSetState& state,
                               ReceiverId id,
                               std::unique_ptr<ReceiverState> receiver,
                               std::unique_ptr<MessageFilter> filter)
    : state_(state), id_(id), receiver_(std::move(receiver)) {
  receiver_->InstallDispatchHooks(
      std::make_unique<DispatchFilter>(*this, std::move(filter)),
      base::BindRepeating(&ReceiverSetState::Entry::OnDisconnect,
                          base::Unretained(this)));
}

ReceiverSetState::Entry::~Entry() = default;

void ReceiverSetState::Entry::WillDispatch() {
  state_.SetDispatchContext(receiver_->GetContext(), id_);
}

void ReceiverSetState::Entry::DidDispatchOrReject() {
  state_.SetDispatchContext(nullptr, 0);
}

void ReceiverSetState::Entry::OnDisconnect(uint32_t custom_reason_code,
                                           const std::string& description) {
  WillDispatch();
  state_.OnDisconnect(id_, custom_reason_code, description);
}

ReceiverSetState::ReceiverSetState() = default;

ReceiverSetState::~ReceiverSetState() = default;

void ReceiverSetState::set_disconnect_handler(base::RepeatingClosure handler) {
  disconnect_handler_ = std::move(handler);
  disconnect_with_reason_handler_.Reset();
}

void ReceiverSetState::set_disconnect_with_reason_handler(
    RepeatingConnectionErrorWithReasonCallback handler) {
  disconnect_with_reason_handler_ = std::move(handler);
  disconnect_handler_.Reset();
}

ReportBadMessageCallback ReceiverSetState::GetBadMessageCallback() {
  DCHECK(current_context_);
  return base::BindOnce(
      [](ReportBadMessageCallback error_callback,
         base::WeakPtr<ReceiverSetState> receiver_set, ReceiverId receiver_id,
         std::string_view error) {
        std::move(error_callback).Run(error);
        if (receiver_set)
          receiver_set->Remove(receiver_id);
      },
      mojo::GetBadMessageCallback(), weak_ptr_factory_.GetWeakPtr(),
      current_receiver());
}

ReceiverId ReceiverSetState::Add(std::unique_ptr<ReceiverState> receiver,
                                 std::unique_ptr<MessageFilter> filter) {
  ReceiverId id = ++next_receiver_id_;
  CHECK_NE(0u, id) << "ReceiverId overflow";
  entries_.insert({id, std::make_unique<Entry>(*this, id, std::move(receiver),
                                               std::move(filter))});
  return id;
}

bool ReceiverSetState::Remove(ReceiverId id) {
  auto it = entries_.find(id);
  if (it == entries_.end())
    return false;
  entries_.erase(it);
  return true;
}

bool ReceiverSetState::RemoveWithReason(ReceiverId id,
                                        uint32_t custom_reason_code,
                                        const std::string& description) {
  auto it = entries_.find(id);
  if (it == entries_.end())
    return false;
  it->second->receiver().ResetWithReason(custom_reason_code, description);
  entries_.erase(it);
  return true;
}

void ReceiverSetState::FlushForTesting() {
  // We avoid flushing while iterating over |entries_| because this set may be
  // mutated during individual flush operations.  Instead, snapshot the
  // ReceiverIds first, then iterate over them. This is less efficient, but it's
  // only a testing API. This also allows for correct behavior in reentrant
  // calls to FlushForTesting().
  std::vector<ReceiverId> ids;
  for (const auto& entry : entries_)
    ids.push_back(entry.first);

  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  for (const auto& id : ids) {
    if (!weak_self)
      return;
    auto it = entries_.find(id);
    if (it != entries_.end())
      it->second->receiver().FlushForTesting();
  }
}

void ReceiverSetState::SetDispatchContext(void* context,
                                          ReceiverId receiver_id) {
  current_context_ = context;
  current_receiver_ = receiver_id;
}

void ReceiverSetState::OnDisconnect(ReceiverId id,
                                    uint32_t custom_reason_code,
                                    const std::string& description) {
  auto it = entries_.find(id);
  CHECK(it != entries_.end(), base::NotFatalUntil::M130);

  // We keep the Entry alive throughout error dispatch.
  std::unique_ptr<Entry> entry = std::move(it->second);
  entries_.erase(it);

  if (disconnect_handler_)
    disconnect_handler_.Run();
  else if (disconnect_with_reason_handler_)
    disconnect_with_reason_handler_.Run(custom_reason_code, description);
}

}  // namespace mojo
