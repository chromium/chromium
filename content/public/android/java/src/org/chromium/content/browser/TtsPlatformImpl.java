// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * This class is the Java counterpart to the C++ TtsPlatformImplAndroid class.
 * It implements the Android-native text-to-speech code to support the web
 * speech synthesis API.
 *
 * Threading model note: all calls from C++ must happen on the UI thread.
 * Callbacks from Android may happen on a different thread, so we always
 * use PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, ...)  when calling back to C++.
 */
@JNINamespace("content")
class TtsPlatformImpl {
    private static class TtsVoice {
        private final String mName;
        private final String mLanguage;

        private TtsVoice(String name, String language) {
            mName = name;
            mLanguage = language;
        }
    }

    private static class PendingUtterance {
        TtsPlatformImpl mImpl;
        int mUtteranceId;
        String mText;
        String mLang;
        String mEngineId;
        float mRate;
        float mPitch;
        float mVolume;

        private PendingUtterance(
                TtsPlatformImpl impl,
                int utteranceId,
                String text,
                String lang,
                String engineId,
                float rate,
                float pitch,
                float volume) {
            mImpl = impl;
            mUtteranceId = utteranceId;
            mText = text;
            mLang = lang;
            mRate = rate;
            mPitch = pitch;
            mVolume = volume;
            mEngineId = engineId;
        }

        private void speak() {
            mImpl.speak(mUtteranceId, mText, mLang, mEngineId, mRate, mPitch, mVolume);
        }
    }

    private static class TtsEngine {
        private TextToSpeech mTextToSpeech;
        private @Nullable List<TtsVoice> mVoices;
        private boolean mInitialized;
        private @Nullable String mCurrentLanguage;
        private @Nullable PendingUtterance mPendingUtterance;
        private long mNativeTtsPlatformImplAndroid;

        /** Constructor with the default TTS Engine */
        private TtsEngine(long nativeTtsPlatformImplAndroid) {
            mNativeTtsPlatformImplAndroid = nativeTtsPlatformImplAndroid;
            mInitialized = false;
            mTextToSpeech =
                    new TextToSpeech(
                            ContextUtils.getApplicationContext(),
                            status -> {
                                if (status == TextToSpeech.SUCCESS) {
                                    PostTask.runOrPostTask(
                                            TaskTraits.UI_DEFAULT, () -> initializeDefault());
                                }
                            });
        }

        /**
         * Constructor for a specific TTS Engine with package name
         * @param engineId Package name for the TTS Engine to be used.
         */
        private TtsEngine(String engineId) {
            mInitialized = false;
            mTextToSpeech =
                    new TextToSpeech(
                            ContextUtils.getApplicationContext(),
                            status -> {
                                if (status == TextToSpeech.SUCCESS) {
                                    initializeNonDefault();
                                }
                            },
                            engineId);
        }

        /** Initialization for non-default TTS Engine does not enumerate voices. */
        private void initializeNonDefault() {
            mInitialized = true;
            if (mPendingUtterance != null) mPendingUtterance.speak();
        }

        /**
         * Note: we enforce that this method is called on the UI thread, so
         * we can call TtsPlatformImplJni.get().voicesChanged directly.
         */
        private void initializeDefault() {
            TraceEvent.startAsync("TtsEngine:initialize_default", hashCode());

            new AsyncTask<List<TtsVoice>>() {
                @Override
                protected List<TtsVoice> doInBackground() {
                    assert mNativeTtsPlatformImplAndroid != 0;

                    try (TraceEvent te =
                            TraceEvent.scoped("TtsEngine:initialize_default.async_task")) {
                        Locale[] locales = Locale.getAvailableLocales();
                        final List<TtsVoice> voices = new ArrayList<>();
                        for (Locale locale : locales) {
                            if (!locale.getVariant().isEmpty()) continue;
                            try {
                                if (mTextToSpeech.isLanguageAvailable(locale) > 0) {
                                    String name = locale.getDisplayLanguage();
                                    if (!locale.getCountry().isEmpty()) {
                                        name += " " + locale.getDisplayCountry();
                                    }
                                    TtsVoice voice = new TtsVoice(name, locale.toString());
                                    voices.add(voice);
                                }
                            } catch (Exception e) {
                                // Just skip the locale if it's invalid.
                                //
                                // We used to catch only java.util.MissingResourceException,
                                // but we need to catch more exceptions to work around a bug
                                // in Google TTS when we query "bn".
                                // http://crbug.com/792856
                            }
                        }
                        return voices;
                    }
                }

                @Override
                protected void onPostExecute(List<TtsVoice> voices) {
                    mVoices = voices;
                    mInitialized = true;

                    TtsPlatformImplJni.get().voicesChanged(mNativeTtsPlatformImplAndroid);

                    if (mPendingUtterance != null) mPendingUtterance.speak();

                    TraceEvent.finishAsync(
                            "TtsEngine:initialize_default", TtsEngine.this.hashCode());
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        private boolean isInitialized() {
            return mInitialized;
        }

        private void setPendingUtterance(PendingUtterance pendingUtterance) {
            mPendingUtterance = pendingUtterance;
        }

        private void clearPendingUtterance() {
            mPendingUtterance = null;
        }

        private boolean speak(
                int utteranceId, String text, String lang, float rate, float pitch, float volume) {
            if (!isInitialized()) {
                return false;
            }
            if (lang == null) {
                mCurrentLanguage = null;
            } else if (!TextUtils.equals(lang, mCurrentLanguage)) {
                mTextToSpeech.setLanguage(LocaleUtils.forLanguageTag(lang.replace("_", "-")));
                mCurrentLanguage = lang;
            }

            mTextToSpeech.setSpeechRate(rate);
            mTextToSpeech.setPitch(pitch);
            Bundle params = new Bundle();
            if (volume != 1.0) {
                params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, volume);
            }
            int result =
                    mTextToSpeech.speak(
                            text, TextToSpeech.QUEUE_FLUSH, params, Integer.toString(utteranceId));
            return (result == TextToSpeech.SUCCESS);
        }

        private void stop() {
            if (isInitialized()) mTextToSpeech.stop();
            if (mPendingUtterance != null) mPendingUtterance = null;
        }

        private TextToSpeech getTextToSpeech() {
            return mTextToSpeech;
        }

        private List<TtsVoice> getVoices() {
            return mVoices;
        }
    }

    private long mNativeTtsPlatformImplAndroid;
    private final TtsEngine mDefaultTtsEngine;
    private final Map<String, TtsEngine> mNonDefaultTtsEnginesMap;

    private TtsPlatformImpl(long nativeTtsPlatformImplAndroid) {
        mNativeTtsPlatformImplAndroid = nativeTtsPlatformImplAndroid;
        mDefaultTtsEngine = new TtsEngine(mNativeTtsPlatformImplAndroid);
        mNonDefaultTtsEnginesMap = new HashMap<String, TtsEngine>();
        addOnUtteranceProgressListener(mDefaultTtsEngine.getTextToSpeech());
    }

    private boolean isEngineInstalled(String engineId) {
        for (TextToSpeech.EngineInfo engineInfo :
                mDefaultTtsEngine.getTextToSpeech().getEngines()) {
            if (TextUtils.equals(engineInfo.name, engineId)) return true;
        }
        return false;
    }

    private TtsEngine getOrCreateTtsEngine(String engineId) {
        if (!mDefaultTtsEngine.isInitialized()
                || TextUtils.isEmpty(engineId)
                || TextUtils.equals(
                        engineId, mDefaultTtsEngine.getTextToSpeech().getDefaultEngine())
                || !isEngineInstalled(engineId)) {
            return mDefaultTtsEngine;
        }
        if (mNonDefaultTtsEnginesMap.containsKey(engineId)) {
            return mNonDefaultTtsEnginesMap.get(engineId);
        }

        TtsEngine ttsEngine = new TtsEngine(engineId);
        addOnUtteranceProgressListener(ttsEngine.getTextToSpeech());
        mNonDefaultTtsEnginesMap.put(engineId, ttsEngine);
        return ttsEngine;
    }

    /**
     * Create a TtsPlatformImpl object, which is owned by TtsPlatformImplAndroid
     * on the C++ side.
     *  @param nativeTtsPlatformImplAndroid The C++ object that owns us.
     *
     */
    @CalledByNative
    private static TtsPlatformImpl create(long nativeTtsPlatformImplAndroid) {
        return new TtsPlatformImpl(nativeTtsPlatformImplAndroid);
    }

    /**
     * Called when our C++ counterpoint is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
        mNativeTtsPlatformImplAndroid = 0;
    }

    /**
     * @return true if our TextToSpeech object is initialized and we've
     * finished scanning the list of voices.
     */
    @CalledByNative
    private boolean isInitialized() {
        return mDefaultTtsEngine.isInitialized();
    }

    /**
     * @return the number of voices.
     */
    @CalledByNative
    private int getVoiceCount() {
        assert mDefaultTtsEngine.isInitialized();
        return mDefaultTtsEngine.getVoices().size();
    }

    /**
     * @return the name of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceName(int voiceIndex) {
        assert mDefaultTtsEngine.isInitialized();
        return mDefaultTtsEngine.getVoices().get(voiceIndex).mName;
    }

    /**
     * @return the language of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceLanguage(int voiceIndex) {
        assert mDefaultTtsEngine.isInitialized();
        return mDefaultTtsEngine.getVoices().get(voiceIndex).mLanguage;
    }

    /**
     * Attempt to start speaking an utterance. If it returns true, will call back on
     * start and end.
     *
     * @param utteranceId A unique id for this utterance so that callbacks can be tied
     *     to a particular utterance.
     * @param text The text to speak.
     * @param lang The language code for the text (e.g., "en-US").
     * @param engineId The ID of the underlying TTS engine to use for this utterance.
     *     If not specified or we are unable to create the engine, we use the default
     *     engine.
     * @param rate The speech rate, in the units expected by Android TextToSpeech.
     * @param pitch The speech pitch, in the units expected by Android TextToSpeech.
     * @param volume The speech volume, in the units expected by Android TextToSpeech.
     * @return true on success.
     */
    @CalledByNative
    private boolean speak(
            int utteranceId,
            String text,
            String lang,
            String engineId,
            float rate,
            float pitch,
            float volume) {
        TtsEngine ttsEngine = getOrCreateTtsEngine(engineId);
        if (!ttsEngine.isInitialized()) {
            clearPendingUtterances();
            PendingUtterance pendingUtterance =
                    new PendingUtterance(
                            this, utteranceId, text, lang, engineId, rate, pitch, volume);
            ttsEngine.setPendingUtterance(pendingUtterance);
            return true;
        }

        return ttsEngine.speak(utteranceId, text, lang, rate, pitch, volume);
    }

    /** Stop the current utterance. */
    @CalledByNative
    private void stop() {
        mDefaultTtsEngine.stop();
        for (Map.Entry<String, TtsEngine> entry : mNonDefaultTtsEnginesMap.entrySet()) {
            entry.getValue().stop();
        }
    }

    private void clearPendingUtterances() {
        mDefaultTtsEngine.clearPendingUtterance();
        for (Map.Entry<String, TtsEngine> entry : mNonDefaultTtsEnginesMap.entrySet()) {
            entry.getValue().clearPendingUtterance();
        }
    }

    /** Post a task to the UI thread to send the TTS "end" event. */
    private void sendEndEventOnUiThread(final String utteranceId) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeTtsPlatformImplAndroid != 0) {
                        TtsPlatformImplJni.get()
                                .onEndEvent(
                                        mNativeTtsPlatformImplAndroid,
                                        Integer.parseInt(utteranceId));
                    }
                });
    }

    /** Post a task to the UI thread to send the TTS "error" event. */
    private void sendErrorEventOnUiThread(final String utteranceId) {
        if (TextUtils.isEmpty(utteranceId)) {
            // An empty string here is not supposed to happen, but crashes dictate that it does.
            // https://crbug.com/40922353
            return;
        }
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeTtsPlatformImplAndroid != 0) {
                        TtsPlatformImplJni.get()
                                .onErrorEvent(
                                        mNativeTtsPlatformImplAndroid,
                                        Integer.parseInt(utteranceId));
                    }
                });
    }

    /** Post a task to the UI thread to send the TTS "start" event. */
    private void sendStartEventOnUiThread(final String utteranceId) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeTtsPlatformImplAndroid != 0) {
                        TtsPlatformImplJni.get()
                                .onStartEvent(
                                        mNativeTtsPlatformImplAndroid,
                                        Integer.parseInt(utteranceId));
                    }
                });
    }

    @SuppressWarnings("deprecation")
    private void addOnUtteranceProgressListener(TextToSpeech tts) {
        tts.setOnUtteranceProgressListener(
                new UtteranceProgressListener() {
                    @Override
                    public void onDone(final String utteranceId) {
                        sendEndEventOnUiThread(utteranceId);
                    }

                    @Override
                    public void onError(final String utteranceId, int errorCode) {
                        sendErrorEventOnUiThread(utteranceId);
                    }

                    @Override
                    @Deprecated
                    public void onError(final String utteranceId) {}

                    @Override
                    public void onStart(final String utteranceId) {
                        sendStartEventOnUiThread(utteranceId);
                    }
                });
    }

    @NativeMethods
    interface Natives {
        void voicesChanged(long nativeTtsPlatformImplAndroid);

        void onEndEvent(long nativeTtsPlatformImplAndroid, int utteranceId);

        void onStartEvent(long nativeTtsPlatformImplAndroid, int utteranceId);

        void onErrorEvent(long nativeTtsPlatformImplAndroid, int utteranceId);
    }
}
