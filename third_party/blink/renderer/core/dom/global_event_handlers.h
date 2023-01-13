/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GLOBAL_EVENT_HANDLERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GLOBAL_EVENT_HANDLERS_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GlobalEventHandlers {
  STATIC_ONLY(GlobalEventHandlers);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(aftertoggle, kAftertoggle)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(animationend, kAnimationend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(animationiteration,
                                         kAnimationiteration)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(animationstart, kAnimationstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(auxclick, kAuxclick)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(beforeinput, kBeforeinput)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(beforematch, kBeforematch)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(beforetoggle, kBeforetoggle)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(blur, kBlur)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(canplay, kCanplay)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(canplaythrough, kCanplaythrough)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(click, kClick)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(close, kClose)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(contentvisibilityautostatechange,
                                         kContentvisibilityautostatechange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(contextmenu, kContextmenu)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(contextlost, kContextlost)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(contextrestored, kContextrestored)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cuechange, kCuechange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dblclick, kDblclick)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(drag, kDrag)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragend, kDragend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragenter, kDragenter)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragleave, kDragleave)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragover, kDragover)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragstart, kDragstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(drop, kDrop)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(durationchange, kDurationchange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(emptied, kEmptied)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(focus, kFocus)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(formdata, kFormdata)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(gotpointercapture, kGotpointercapture)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(input, kInput)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(invalid, kInvalid)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keydown, kKeydown)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keypress, kKeypress)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keyup, kKeyup)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(load, kLoad)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadeddata, kLoadeddata)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadedmetadata, kLoadedmetadata)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadstart, kLoadstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(lostpointercapture,
                                         kLostpointercapture)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousedown, kMousedown)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseenter, kMouseenter)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseleave, kMouseleave)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousemove, kMousemove)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseout, kMouseout)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseover, kMouseover)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseup, kMouseup)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousewheel, kMousewheel)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(overscroll, kOverscroll)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pause, kPause)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(play, kPlay)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(playing, kPlaying)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointercancel, kPointercancel)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerdown, kPointerdown)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerenter, kPointerenter)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerleave, kPointerleave)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointermove, kPointermove)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerout, kPointerout)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerover, kPointerover)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerrawupdate, kPointerrawupdate)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerup, kPointerup)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(popoverhide, kPopoverhide)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(popovershow, kPopovershow)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(progress, kProgress)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(ratechange, kRatechange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(reset, kReset)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(resize, kResize)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(scroll, kScroll)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(scrollend, kScrollend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation,
                                         kSecuritypolicyviolation)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(seeked, kSeeked)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(seeking, kSeeking)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(select, kSelect)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(selectionchange, kSelectionchange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(selectstart, kSelectstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(slotchange, kSlotchange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(stalled, kStalled)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(submit, kSubmit)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(suspend, kSuspend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(timeupdate, kTimeupdate)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(toggle, kToggle)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchcancel, kTouchcancel)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchend, kTouchend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchmove, kTouchmove)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchstart, kTouchstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(transitioncancel, kTransitioncancel)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(transitionend, kTransitionend)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(transitionrun, kTransitionrun)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(transitionstart, kTransitionstart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(volumechange, kVolumechange)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(waiting, kWaiting)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitanimationend,
                                         kWebkitAnimationEnd)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitanimationiteration,
                                         kWebkitAnimationIteration)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitanimationstart,
                                         kWebkitAnimationStart)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkittransitionend,
                                         kWebkitTransitionEnd)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(wheel, kWheel)
};

}  // namespace

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_GLOBAL_EVENT_HANDLERS_H_
