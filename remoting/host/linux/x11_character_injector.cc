// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_character_injector.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "remoting/host/linux/x11_keyboard.h"

namespace {

constexpr base::TimeDelta kMappingExpireDuration = base::Milliseconds(200);

}  // namespace

namespace remoting {

struct X11CharacterInjector::KeyInfo {
  uint32_t keycode;
  base::TimeTicks expire_at;
};

struct X11CharacterInjector::MapResult {
  bool success;

  uint32_t keycode;
  uint32_t modifiers;

  // If success == false and |retry_after| is not zero, user may retry
  // AddNewCharacter() after |retry_after| has elapsed.
  base::TimeDelta retry_after;
};

X11CharacterInjector::X11CharacterInjector(
    std::unique_ptr<X11Keyboard> keyboard)
    : keyboard_(std::move(keyboard)) {
  std::vector<uint32_t> keycodes = keyboard_->GetUnusedKeycodes();
  for (uint32_t keycode : keycodes) {
    available_keycodes_.push_back({keycode, base::TimeTicks()});
  }
}

X11CharacterInjector::~X11CharacterInjector() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Clear all used key mappings.
  for (const KeyInfo& info : available_keycodes_) {
    if (!info.expire_at.is_null()) {
      keyboard_->ChangeKeyMapping(info.keycode, 0);
    }
  }
  keyboard_->Sync();
}

void X11CharacterInjector::Inject(uint32_t code_point) {
  DCHECK(thread_checker_.CalledOnValidThread());
  characters_queue_.push(code_point);
  Schedule(base::TimeDelta());
}

void X11CharacterInjector::Schedule(base::TimeDelta delay) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (injection_timer_.IsRunning()) {
    return;
  }
  injection_timer_.Start(FROM_HERE, delay, this,
                         &X11CharacterInjector::DoInject);
}

void X11CharacterInjector::DoInject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  while (!characters_queue_.empty()) {
    uint32_t code_point = characters_queue_.front();
    MapResult result = MapCharacter(code_point);
    if (!result.success) {
      if (result.retry_after.is_zero()) {
        characters_queue_.pop();
        continue;
      }
      Schedule(result.retry_after);
      break;
    }
    keyboard_->PressKey(result.keycode, result.modifiers);
    characters_queue_.pop();
  }

  keyboard_->Flush();
}

X11CharacterInjector::MapResult X11CharacterInjector::MapCharacter(
    uint32_t code_point) {
  MapResult result{false, 0, 0, base::TimeDelta()};

  base::TimeTicks now = base::TimeTicks::Now();

  if (keyboard_->FindKeycode(code_point, &result.keycode, &result.modifiers)) {
    uint32_t keycode = result.keycode;
    auto position =
        base::ranges::find(available_keycodes_, keycode, &KeyInfo::keycode);
    if (position != available_keycodes_.end()) {
      ResetKeyInfoExpirationTime(now, position);
    }
    result.success = true;
    return result;
  }

  if (available_keycodes_.empty()) {
    return result;
  }

  KeyInfo& info = available_keycodes_.front();
  if (info.expire_at > now) {
    result.retry_after = info.expire_at - now;
    return result;
  }

  if (!keyboard_->ChangeKeyMapping(info.keycode, code_point)) {
    return result;
  }

  result.success = true;
  result.keycode = info.keycode;

  // Modifiers can always be 0 since |code_point| is mapped to both upper and
  // lower case.

  ResetKeyInfoExpirationTime(now, available_keycodes_.begin());

  keyboard_->Sync();
  return result;
}

void X11CharacterInjector::ResetKeyInfoExpirationTime(
    base::TimeTicks now,
    std::vector<KeyInfo>::iterator position) {
  KeyInfo info = *position;
  info.expire_at = now + kMappingExpireDuration;
  available_keycodes_.erase(position);
  available_keycodes_.push_back(info);
}

}  // namespace remoting
