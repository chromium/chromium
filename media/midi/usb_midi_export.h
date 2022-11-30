// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_EXPORT_H_
#define MEDIA_MIDI_USB_MIDI_EXPORT_H_

// These files can be included as part of the midi component using the
// MIDI_IMPLEMENTATION define (where we want to export in the component build),
// or in the unit tests (where we never want to export/import, even in
// component mode). The EXPORT_USB_MIDI define controls this.
#if defined(COMPONENT_BUILD) && defined(EXPORT_USB_MIDI)
#if defined(WIN32)

#if defined(MIDI_IMPLEMENTATION)
#define USB_MIDI_EXPORT __declspec(dllexport)
#else
#define USB_MIDI_EXPORT __declspec(dllimport)
#endif  // defined(MIDI_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MIDI_IMPLEMENTATION)
#define USB_MIDI_EXPORT __attribute__((visibility("default")))
#else
#define USB_MIDI_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define USB_MIDI_EXPORT
#endif

#endif  // MEDIA_MIDI_USB_MIDI_EXPORT_H_
