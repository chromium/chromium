/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "third_party/blink/renderer/platform/mac/color_mac.h"

#import <AppKit/AppKit.h>

namespace blink {

// NSColor calls don't throw, so no need to block Cocoa exceptions in this file

NSColor* NsColor(const Color& color) {
  RGBA32 c = color.Rgb();
  if (color == Color::kTransparent) {
    // Need this to avoid returning nil because cachedRGBAValues will default
    // to 0.
    static NSColor* clear_color = [NSColor colorWithDeviceRed:0
                                                        green:0
                                                         blue:0
                                                        alpha:0];
    return clear_color;
  } else if (color == Color::kBlack) {
    static NSColor* black_color = [NSColor colorWithDeviceRed:0
                                                        green:0
                                                         blue:0
                                                        alpha:1];
    return black_color;
  } else if (color == Color::kWhite) {
    static NSColor* white_color = [NSColor colorWithDeviceRed:1
                                                        green:1
                                                         blue:1
                                                        alpha:1];
    return white_color;
  } else {
    const int kCacheSize = 32;
    static unsigned cached_rgba_values[kCacheSize];
    static NSColor* cached_colors[kCacheSize];

    for (int i = 0; i != kCacheSize; ++i) {
      if (cached_rgba_values[i] == c)
        return cached_colors[i];
    }

    NSColor* result = [NSColor
        colorWithDeviceRed:static_cast<CGFloat>(color.Red()) / 255
                     green:static_cast<CGFloat>(color.Green()) / 255
                      blue:static_cast<CGFloat>(color.Blue()) / 255
                     alpha:static_cast<CGFloat>(color.AlphaAsInteger()) / 255];

    static int cursor;
    cached_rgba_values[cursor] = c;
    cached_colors[cursor] = result;
    if (++cursor == kCacheSize) {
      cursor = 0;
    }
    return result;
  }
}

}  // namespace blink
