/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "third_party/blink/renderer/core/editing/kill_ring.h"

namespace blink {

extern "C" {

// Kill ring calls. Would be better to use NSKillRing.h, but that's not
// available as API or SPI.

void _NSInitializeKillRing();
void _NSAppendToKillRing(NSString*);
void _NSPrependToKillRing(NSString*);
NSString* _NSYankFromKillRing();
void _NSNewKillRingSequence();
void _NSSetKillRingToYankedState();
}

static void InitializeKillRingIfNeeded() {
  static bool initialized_kill_ring = false;
  if (!initialized_kill_ring) {
    initialized_kill_ring = true;
    _NSInitializeKillRing();
  }
}

void KillRing::Append(const String& string) {
  InitializeKillRingIfNeeded();
  _NSAppendToKillRing(string);
}

void KillRing::Prepend(const String& string) {
  InitializeKillRingIfNeeded();
  _NSPrependToKillRing(string);
}

String KillRing::Yank() {
  InitializeKillRingIfNeeded();
  return _NSYankFromKillRing();
}

void KillRing::StartNewSequence() {
  InitializeKillRingIfNeeded();
  _NSNewKillRingSequence();
}

void KillRing::SetToYankedState() {
  InitializeKillRingIfNeeded();
  _NSSetKillRingToYankedState();
}

}  // namespace blink
