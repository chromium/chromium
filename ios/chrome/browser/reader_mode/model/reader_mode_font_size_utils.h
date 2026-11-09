// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_FONT_SIZE_UTILS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_FONT_SIZE_UTILS_H_

namespace web_distiller {
class DistilledPagePrefs;
}

// Increases the font size.
void IncreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

// Decreases the font size.
void DecreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

// Resets the font size to its default value.
void ResetReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

// Returns true if the font size can be increased.
bool CanIncreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

// Returns true if the font size can be decreased.
bool CanDecreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

// Returns true if the font size can be reset.
bool CanResetReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs);

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_FONT_SIZE_UTILS_H_
