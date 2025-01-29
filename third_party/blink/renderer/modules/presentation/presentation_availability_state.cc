// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"

#include <algorithm>

#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"

namespace blink {

PresentationAvailabilityState::PresentationAvailabilityState(
    mojom::blink::PresentationService* presentation_service)
    : presentation_service_(presentation_service) {}

PresentationAvailabilityState::~PresentationAvailabilityState() = default;

void PresentationAvailabilityState::RequestAvailability(
    PresentationAvailability* availability) {
  const auto& urls = availability->Urls();
  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    listener = MakeGarbageCollected<AvailabilityListener>(urls);
    availability_listeners_.emplace_back(listener);
  }

  listener->availabilities.insert(availability);

  for (const auto& availability_url : urls) {
    StartListeningToURL(availability_url);
  }
}

void PresentationAvailabilityState::AddObserver(
    PresentationAvailabilityObserver* observer) {
  const auto& urls = observer->Urls();
  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    listener = MakeGarbageCollected<AvailabilityListener>(urls);
    availability_listeners_.emplace_back(listener);
  }

  if (listener->availability_observers.Contains(observer)) {
    return;
  }

  listener->availability_observers.push_back(observer);
  for (const auto& availability_url : urls) {
    StartListeningToURL(availability_url);
  }
}

void PresentationAvailabilityState::RemoveObserver(
    PresentationAvailabilityObserver* observer) {
  const auto& urls = observer->Urls();
  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    DLOG(WARNING) << "Stop listening for availability for unknown URLs.";
    return;
  }

  wtf_size_t slot = listener->availability_observers.Find(observer);
  if (slot != kNotFound) {
    listener->availability_observers.EraseAt(slot);
  }
  for (const auto& availability_url : urls) {
    MaybeStopListeningToURL(availability_url);
  }

  TryRemoveAvailabilityListener(listener);
}

void PresentationAvailabilityState::UpdateAvailability(
    const KURL& url,
    mojom::blink::ScreenAvailability availability) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status) {
    return;
  }

  if (listening_status->listening_state == ListeningState::kWaiting) {
    listening_status->listening_state = ListeningState::kActive;
  }

  if (listening_status->last_known_availability == availability) {
    return;
  }

  listening_status->last_known_availability = availability;

  HeapVector<Member<AvailabilityListener>> listeners = availability_listeners_;
  for (auto& listener : listeners) {
    if (!listener->urls.Contains<KURL>(url)) {
      continue;
    }

    auto screen_availability = GetScreenAvailability(listener->urls);
    DCHECK(screen_availability != mojom::blink::ScreenAvailability::UNKNOWN);
    HeapVector<Member<PresentationAvailabilityObserver>> observers =
        listener->availability_observers;
    for (auto& observer : observers) {
      observer->AvailabilityChanged(screen_availability);
    }

    if (screen_availability == mojom::blink::ScreenAvailability::DISABLED) {
      for (auto& availability_ptr : listener->availabilities) {
        availability_ptr->RejectPendingPromises();
      }
    } else {
      for (auto& availability_ptr : listener->availabilities) {
        availability_ptr->ResolvePendingPromises();
      }
    }
    listener->availabilities.clear();

    for (const auto& availability_url : listener->urls) {
      MaybeStopListeningToURL(availability_url);
    }

    TryRemoveAvailabilityListener(listener);
  }
}

mojom::blink::ScreenAvailability
PresentationAvailabilityState::GetScreenAvailability(
    const Vector<KURL>& urls) const {
  bool has_disabled = false;
  bool has_source_not_supported = false;
  bool has_unavailable = false;

  for (const auto& url : urls) {
    auto* status = GetListeningStatus(url);
    auto screen_availability = status
                                   ? status->last_known_availability
                                   : mojom::blink::ScreenAvailability::UNKNOWN;
    switch (screen_availability) {
      case mojom::blink::ScreenAvailability::AVAILABLE:
        return mojom::blink::ScreenAvailability::AVAILABLE;
      case mojom::blink::ScreenAvailability::DISABLED:
        has_disabled = true;
        break;
      case mojom::blink::ScreenAvailability::SOURCE_NOT_SUPPORTED:
        has_source_not_supported = true;
        break;
      case mojom::blink::ScreenAvailability::UNAVAILABLE:
        has_unavailable = true;
        break;
      case mojom::blink::ScreenAvailability::UNKNOWN:
        break;
    }
  }

  if (has_disabled) {
    return mojom::blink::ScreenAvailability::DISABLED;
  } else if (has_source_not_supported) {
    return mojom::blink::ScreenAvailability::SOURCE_NOT_SUPPORTED;
  } else if (has_unavailable) {
    return mojom::blink::ScreenAvailability::UNAVAILABLE;
  } else {
    return mojom::blink::ScreenAvailability::UNKNOWN;
  }
}

void PresentationAvailabilityState::Trace(Visitor* visitor) const {
  visitor->Trace(availability_listeners_);
}

void PresentationAvailabilityState::StartListeningToURL(const KURL& url) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status) {
    listening_status = new ListeningStatus(url);
    availability_listening_status_.emplace_back(listening_status);
  }

  // Already listening.
  if (listening_status->listening_state != ListeningState::kInactive) {
    return;
  }

  listening_status->listening_state = ListeningState::kWaiting;
  presentation_service_->ListenForScreenAvailability(url);
}

void PresentationAvailabilityState::MaybeStopListeningToURL(const KURL& url) {
  for (const auto& listener : availability_listeners_) {
    if (!listener->urls.Contains(url)) {
      continue;
    }

    // URL is still observed by some availability object.
    if (!listener->availabilities.empty() ||
        !listener->availability_observers.empty()) {
      return;
    }
  }

  auto status_it = std::ranges::find(availability_listening_status_, url,
                                     &ListeningStatus::url);
  if (status_it == availability_listening_status_.end()) {
    LOG(WARNING) << "Stop listening to unknown url: " << url.GetString();
  } else {
    // Delete ListeningStatus object if there are no availability objects
    // associated with the URL.
    availability_listening_status_.erase(status_it);
    presentation_service_->StopListeningForScreenAvailability(url);
  }
}

PresentationAvailabilityState::AvailabilityListener*
PresentationAvailabilityState::GetAvailabilityListener(
    const Vector<KURL>& urls) {
  auto listener_it = std::ranges::find(availability_listeners_, urls,
                                       &AvailabilityListener::urls);
  return listener_it == availability_listeners_.end() ? nullptr : *listener_it;
}

void PresentationAvailabilityState::TryRemoveAvailabilityListener(
    AvailabilityListener* listener) {
  // URL is still observed by some availability object.
  if (!listener->availabilities.empty() ||
      !listener->availability_observers.empty()) {
    return;
  }

  wtf_size_t slot = availability_listeners_.Find(listener);
  if (slot != kNotFound) {
    availability_listeners_.EraseAt(slot);
  }
}

PresentationAvailabilityState::ListeningStatus*
PresentationAvailabilityState::GetListeningStatus(const KURL& url) const {
  auto status_it = std::ranges::find(availability_listening_status_, url,
                                     &ListeningStatus::url);
  return status_it == availability_listening_status_.end() ? nullptr
                                                           : status_it->get();
}

PresentationAvailabilityState::AvailabilityListener::AvailabilityListener(
    const Vector<KURL>& availability_urls)
    : urls(availability_urls) {}

PresentationAvailabilityState::AvailabilityListener::~AvailabilityListener() =
    default;

void PresentationAvailabilityState::AvailabilityListener::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(availabilities);
  visitor->Trace(availability_observers);
}

PresentationAvailabilityState::ListeningStatus::ListeningStatus(
    const KURL& availability_url)
    : url(availability_url),
      last_known_availability(mojom::blink::ScreenAvailability::UNKNOWN),
      listening_state(ListeningState::kInactive) {}

PresentationAvailabilityState::ListeningStatus::~ListeningStatus() = default;

}  // namespace blink
