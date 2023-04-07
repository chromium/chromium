/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

namespace blink {

static protocol::Response ToResponse(ExceptionState& exception_state) {
  if (!exception_state.HadException())
    return protocol::Response::Success();

  String name_prefix = IsDOMExceptionCode(exception_state.Code())
                           ? DOMException::GetErrorName(
                                 exception_state.CodeAs<DOMExceptionCode>()) +
                                 " "
                           : g_empty_string;
  String msg = name_prefix + exception_state.Message();
  return protocol::Response::ServerError(msg.Utf8());
}

InspectorDOMStorageAgent::InspectorDOMStorageAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      enabled_(&agent_state_, /*default_value=*/false) {}

InspectorDOMStorageAgent::~InspectorDOMStorageAgent() = default;

void InspectorDOMStorageAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorDOMStorageAgent::Restore() {
  if (enabled_.Get())
    InnerEnable();
}

void InspectorDOMStorageAgent::InnerEnable() {
  StorageController::GetInstance()->AddLocalStorageInspectorStorageAgent(this);
  StorageNamespace* ns =
      StorageNamespace::From(inspected_frames_->Root()->GetPage());
  if (ns)
    ns->AddInspectorStorageAgent(this);
}

protocol::Response InspectorDOMStorageAgent::enable() {
  if (enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(true);
  InnerEnable();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMStorageAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(false);
  StorageController::GetInstance()->RemoveLocalStorageInspectorStorageAgent(
      this);
  StorageNamespace* ns =
      StorageNamespace::From(inspected_frames_->Root()->GetPage());
  if (ns)
    ns->RemoveInspectorStorageAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMStorageAgent::clear(
    std::unique_ptr<protocol::DOMStorage::StorageId> storage_id) {
  StorageArea* storage_area = nullptr;
  protocol::Response response =
      FindStorageArea(std::move(storage_id), storage_area);
  if (!response.IsSuccess())
    return response;
  DummyExceptionStateForTesting exception_state;
  storage_area->clear(exception_state);
  if (exception_state.HadException())
    return protocol::Response::ServerError("Could not clear the storage");
  return protocol::Response::Success();
}

protocol::Response InspectorDOMStorageAgent::getDOMStorageItems(
    std::unique_ptr<protocol::DOMStorage::StorageId> storage_id,
    std::unique_ptr<protocol::Array<protocol::Array<String>>>* items) {
  StorageArea* storage_area = nullptr;
  protocol::Response response =
      FindStorageArea(std::move(storage_id), storage_area);
  if (!response.IsSuccess())
    return response;

  auto storage_items =
      std::make_unique<protocol::Array<protocol::Array<String>>>();

  DummyExceptionStateForTesting exception_state;
  for (unsigned i = 0; i < storage_area->length(exception_state); ++i) {
    String name(storage_area->key(i, exception_state));
    response = ToResponse(exception_state);
    if (!response.IsSuccess())
      return response;
    String value(storage_area->getItem(name, exception_state));
    response = ToResponse(exception_state);
    if (!response.IsSuccess())
      return response;
    auto entry = std::make_unique<protocol::Array<String>>();
    entry->emplace_back(name);
    entry->emplace_back(value);
    storage_items->emplace_back(std::move(entry));
  }
  *items = std::move(storage_items);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMStorageAgent::setDOMStorageItem(
    std::unique_ptr<protocol::DOMStorage::StorageId> storage_id,
    const String& key,
    const String& value) {
  StorageArea* storage_area = nullptr;
  protocol::Response response =
      FindStorageArea(std::move(storage_id), storage_area);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  storage_area->setItem(key, value, exception_state);
  return ToResponse(exception_state);
}

protocol::Response InspectorDOMStorageAgent::removeDOMStorageItem(
    std::unique_ptr<protocol::DOMStorage::StorageId> storage_id,
    const String& key) {
  StorageArea* storage_area = nullptr;
  protocol::Response response =
      FindStorageArea(std::move(storage_id), storage_area);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  storage_area->removeItem(key, exception_state);
  return ToResponse(exception_state);
}

std::unique_ptr<protocol::DOMStorage::StorageId>
InspectorDOMStorageAgent::GetStorageId(const BlinkStorageKey& storage_key,
                                       bool is_local_storage) {
  return protocol::DOMStorage::StorageId::create()
      .setStorageKey(
          WTF::String(static_cast<StorageKey>(storage_key).Serialize()))
      .setSecurityOrigin(storage_key.GetSecurityOrigin()->ToRawString())
      .setIsLocalStorage(is_local_storage)
      .build();
}

void InspectorDOMStorageAgent::DidDispatchDOMStorageEvent(
    const String& key,
    const String& old_value,
    const String& new_value,
    StorageArea::StorageType storage_type,
    const BlinkStorageKey& storage_key) {
  if (!GetFrontend())
    return;

  std::unique_ptr<protocol::DOMStorage::StorageId> id = GetStorageId(
      storage_key, storage_type == StorageArea::StorageType::kLocalStorage);

  if (key.IsNull())
    GetFrontend()->domStorageItemsCleared(std::move(id));
  else if (new_value.IsNull())
    GetFrontend()->domStorageItemRemoved(std::move(id), key);
  else if (old_value.IsNull())
    GetFrontend()->domStorageItemAdded(std::move(id), key, new_value);
  else
    GetFrontend()->domStorageItemUpdated(std::move(id), key, old_value,
                                         new_value);
}

namespace {
LocalFrame* FrameWithStorageKey(const String& key_raw_string,
                                InspectedFrames& frames) {
  for (LocalFrame* frame : frames) {
    // any frame with given storage key would do, as it's only needed to satisfy
    // the current API
    if (static_cast<StorageKey>(frame->DomWindow()->GetStorageKey())
            .Serialize() == key_raw_string.Utf8())
      return frame;
  }
  return nullptr;
}
}  // namespace

protocol::Response InspectorDOMStorageAgent::FindStorageArea(
    std::unique_ptr<protocol::DOMStorage::StorageId> storage_id,
    StorageArea*& storage_area) {
  String security_origin = storage_id->getSecurityOrigin("");
  String storage_key = storage_id->getStorageKey("");
  bool is_local_storage = storage_id->getIsLocalStorage();
  LocalFrame* const frame =
      !storage_key.empty()
          ? FrameWithStorageKey(storage_key, *inspected_frames_)
          : inspected_frames_->FrameWithSecurityOrigin(security_origin);

  if (!frame) {
    return protocol::Response::ServerError(
        "Frame not found for the given storage id");
  }
  if (is_local_storage) {
    if (!frame->DomWindow()->GetSecurityOrigin()->CanAccessLocalStorage()) {
      return protocol::Response::ServerError(
          "Security origin cannot access local storage");
    }
    storage_area = StorageArea::CreateForInspectorAgent(
        frame->DomWindow(),
        StorageController::GetInstance()->GetLocalStorageArea(
            frame->DomWindow()),
        StorageArea::StorageType::kLocalStorage);
    return protocol::Response::Success();
  }

  if (!frame->DomWindow()->GetSecurityOrigin()->CanAccessSessionStorage()) {
    return protocol::Response::ServerError(
        "Security origin cannot access session storage");
  }
  StorageNamespace* session_namespace =
      StorageNamespace::From(frame->GetPage());
  if (!session_namespace)
    return protocol::Response::ServerError("SessionStorage is not supported");
  DCHECK(session_namespace->IsSessionStorage());

  storage_area = StorageArea::CreateForInspectorAgent(
      frame->DomWindow(), session_namespace->GetCachedArea(frame->DomWindow()),
      StorageArea::StorageType::kSessionStorage);
  return protocol::Response::Success();
}

}  // namespace blink
