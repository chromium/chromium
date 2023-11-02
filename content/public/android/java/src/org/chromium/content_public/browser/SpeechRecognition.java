// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.SpeechRecognitionImpl;

/**
 * This class uses Android's SpeechRecognizer to perform speech recognition for the Web Speech API
 * on Android. Using Android's platform recognizer offers several benefits, like good quality and
 * good local fallback when no data connection is available.
 */
public final class SpeechRecognition {
    private SpeechRecognition() {}

    /**
     * This method must be called before any instance of SpeechRecognition can be created. It will
     * query Android's package manager to find a suitable speech recognition provider that supports
     * continuous recognition.
     */
    public static boolean initialize() {
        return SpeechRecognitionImpl.initialize();
    }
}
