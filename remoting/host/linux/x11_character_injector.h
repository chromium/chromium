// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_CHARACTER_INJECTOR_H_
#define REMOTING_HOST_LINUX_X11_CHARACTER_INJECTOR_H_

#include <stdint.h>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"

namespace remoting {

class X11Keyboard;

// This is a helper class for injecting unicode characters to XWindow server.
// Characters will be queued up and sent when there is available resource.
class X11CharacterInjector {
 public:
  explicit X11CharacterInjector(std::unique_ptr<X11Keyboard> keyboard);

  X11CharacterInjector(const X11CharacterInjector&) = delete;
  X11CharacterInjector& operator=(const X11CharacterInjector&) = delete;

  ~X11CharacterInjector();

  void Inject(uint32_t code_point);

 private:
  struct KeyInfo;
  struct MapResult;

  // Schedules a task to call DoInject after |delay| if no such task has been
  // scheduled.
  void Schedule(base::TimeDelta delay);
  void DoInject();

  // |code_point|: The Unicode code point for the character.
  // If the returned result indicates success, caller can use the returned
  // keycode and modifiers to simulate a key press that can generate the
  // character.
  //
  // Note that the returned result will expire after some amount of time so do
  // not store the result for later use.
  MapResult MapCharacter(uint32_t code_point);

  // Resets the expiration time of the KeyInfo in available_keycodes_[index]
  // to now + the expire duration constant.
  void ResetKeyInfoExpirationTime(base::TimeTicks now,
                                  std::vector<KeyInfo>::iterator position);

  std::unique_ptr<X11Keyboard> keyboard_;
  base::queue<uint32_t> characters_queue_;
  base::OneShotTimer injection_timer_;

  // Sorted by ascending expiration time.
  std::vector<KeyInfo> available_keycodes_;

  base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_CHARACTER_INJECTOR_H_
