// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.os.Build;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/**
 * This class is the Java counterpart to the C++ TtsPlatformImplAndroid class.
 * It implements the Android-native text-to-speech code to support the web
 * speech synthesis API.
 *
 * Threading model note: all calls from C++ must happen on the UI thread.
 * Callbacks from Android may happen on a different thread, so we always
 * use PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, ...)  when calling back to C++.
 */
@JNINamespace("content")
class TtsPlatformImpl implements ActivityStateListener {
    private static class TtsVoice {
        private TtsVoice(String name, String language) {
            mName = name;
            mLanguage = language;
        }
        private final String mName;
        private final String mLanguage;
    }

    private static class PendingUtterance {
        private PendingUtterance(TtsPlatformImpl impl, int utteranceId, String text, String lang,
                float rate, float pitch, float volume) {
            mImpl = impl;
            mUtteranceId = utteranceId;
            mText = text;
            mLang = lang;
            mRate = rate;
            mPitch = pitch;
            mVolume = volume;
        }

        private void speak() {
            mImpl.speak(mUtteranceId, mText, mLang, mRate, mPitch, mVolume);
        }

        TtsPlatformImpl mImpl;
        int mUtteranceId;
        String mText;
        String mLang;
        float mRate;
        float mPitch;
        float mVolume;
    }

    private long mNativeTtsPlatformImplAndroid;
    protected final TextToSpeech mTextToSpeech;
    private boolean mInitialized;
    private List<TtsVoice> mVoices;
    private String mCurrentLanguage;
    private PendingUtterance mPendingUtterance;

    protected TtsPlatformImpl(long nativeTtsPlatformImplAndroid) {
        mInitialized = false;
        mNativeTtsPlatformImplAndroid = nativeTtsPlatformImplAndroid;
        mTextToSpeech = new TextToSpeech(ContextUtils.getApplicationContext(), status -> {
            if (status == TextToSpeech.SUCCESS) {
                PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> initialize());
            }
        });
        addOnUtteranceProgressListener();

        ApplicationStatus.registerStateListenerForAllActivities(this);
    }

    /**
     * Create a TtsPlatformImpl object, which is owned by TtsPlatformImplAndroid
     * on the C++ side.
     *  @param nativeTtsPlatformImplAndroid The C++ object that owns us.
     *
     */
    @CalledByNative
    private static TtsPlatformImpl create(long nativeTtsPlatformImplAndroid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return new LollipopTtsPlatformImpl(nativeTtsPlatformImplAndroid);
        } else {
            return new TtsPlatformImpl(nativeTtsPlatformImplAndroid);
        }
    }

    /**
     * Called when our C++ counterpoint is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
        ApplicationStatus.unregisterActivityStateListener(this);
        mNativeTtsPlatformImplAndroid = 0;
    }

    /**
     * @return true if our TextToSpeech object is initialized and we've
     * finished scanning the list of voices.
     */
    @CalledByNative
    private boolean isInitialized() {
        return mInitialized;
    }

    /**
     * @return the number of voices.
     */
    @CalledByNative
    private int getVoiceCount() {
        assert mInitialized;
        return mVoices.size();
    }

    /**
     * @return the name of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceName(int voiceIndex) {
        assert mInitialized;
        return mVoices.get(voiceIndex).mName;
    }

    /**
     * @return the language of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceLanguage(int voiceIndex) {
        assert mInitialized;
        return mVoices.get(voiceIndex).mLanguage;
    }

    /**
     * Attempt to start speaking an utterance. If it returns true, will call back on
     * start and end.
     *
     * @param utteranceId A unique id for this utterance so that callbacks can be tied
     *     to a particular utterance.
     * @param text The text to speak.
     * @param lang The language code for the text (e.g., "en-US").
     * @param rate The speech rate, in the units expected by Android TextToSpeech.
     * @param pitch The speech pitch, in the units expected by Android TextToSpeech.
     * @param volume The speech volume, in the units expected by Android TextToSpeech.
     * @return true on success.
     */
    @CalledByNative
    private boolean speak(
            int utteranceId, String text, String lang, float rate, float pitch, float volume) {
        // Don't speak when in the background.
        if (!ApplicationStatus.hasVisibleActivities()) return false;

        if (!mInitialized) {
            mPendingUtterance =
                    new PendingUtterance(this, utteranceId, text, lang, rate, pitch, volume);
            return true;
        }
        if (mPendingUtterance != null) mPendingUtterance = null;

        if (!lang.equals(mCurrentLanguage)) {
            mTextToSpeech.setLanguage(new Locale(lang));
            mCurrentLanguage = lang;
        }

        mTextToSpeech.setSpeechRate(rate);
        mTextToSpeech.setPitch(pitch);

        int result = callSpeak(text, volume, utteranceId);
        return (result == TextToSpeech.SUCCESS);
    }

    /**
     * Stop the current utterance.
     */
    @CalledByNative
    private void stop() {
        if (mInitialized) mTextToSpeech.stop();
        if (mPendingUtterance != null) mPendingUtterance = null;
    }

    /**
     * Post a task to the UI thread to send the TTS "end" event.
     */
    protected void sendEndEventOnUiThread(final String utteranceId) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mNativeTtsPlatformImplAndroid != 0) {
                TtsPlatformImplJni.get().onEndEvent(mNativeTtsPlatformImplAndroid,
                        TtsPlatformImpl.this, Integer.parseInt(utteranceId));
            }
        });
    }

    /**
     * Post a task to the UI thread to send the TTS "error" event.
     */
    protected void sendErrorEventOnUiThread(final String utteranceId) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mNativeTtsPlatformImplAndroid != 0) {
                TtsPlatformImplJni.get().onErrorEvent(mNativeTtsPlatformImplAndroid,
                        TtsPlatformImpl.this, Integer.parseInt(utteranceId));
            }
        });
    }

    /**
     * Post a task to the UI thread to send the TTS "start" event.
     */
    protected void sendStartEventOnUiThread(final String utteranceId) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mNativeTtsPlatformImplAndroid != 0) {
                TtsPlatformImplJni.get().onStartEvent(mNativeTtsPlatformImplAndroid,
                        TtsPlatformImpl.this, Integer.parseInt(utteranceId));
            }
        });
    }

    /**
     * This is overridden by LollipopTtsPlatformImpl because the API changed.
     */
    @SuppressWarnings("deprecation")
    protected void addOnUtteranceProgressListener() {
        mTextToSpeech.setOnUtteranceProgressListener(new UtteranceProgressListener() {
            @Override
            public void onDone(final String utteranceId) {
                sendEndEventOnUiThread(utteranceId);
            }

            // This is deprecated in Lollipop and higher but we still need to catch it
            // on pre-Lollipop builds.
            @Override
            @SuppressWarnings("deprecation")
            public void onError(final String utteranceId) {
                sendErrorEventOnUiThread(utteranceId);
            }

            @Override
            public void onStart(final String utteranceId) {
                sendStartEventOnUiThread(utteranceId);
            }
        });
    }

    /**
     * This is overridden by LollipopTtsPlatformImpl because the API changed.
     */
    @SuppressWarnings("deprecation")
    protected int callSpeak(String text, float volume, int utteranceId) {
        HashMap<String, String> params = new HashMap<String, String>();
        if (volume != 1.0) {
            params.put(TextToSpeech.Engine.KEY_PARAM_VOLUME, Double.toString(volume));
        }
        params.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, Integer.toString(utteranceId));
        return mTextToSpeech.speak(text, TextToSpeech.QUEUE_FLUSH, params);
    }

    /**
     * Note: we enforce that this method is called on the UI thread, so
     * we can call TtsPlatformImplJni.get().voicesChanged directly.
     */
    private void initialize() {
        TraceEvent.begin("TtsPlatformImpl:initialize");

        new AsyncTask<List<TtsVoice>>() {
            @Override
            protected List<TtsVoice> doInBackground() {
                assert mNativeTtsPlatformImplAndroid != 0;

                try (TraceEvent te = TraceEvent.scoped("TtsPlatformImpl:initialize.async_task")) {
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

                TtsPlatformImplJni.get().voicesChanged(
                        mNativeTtsPlatformImplAndroid, TtsPlatformImpl.this);

                if (mPendingUtterance != null) mPendingUtterance.speak();

                TraceEvent.end("TtsPlatformImpl:initialize");
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        // Stop speech if all browser windows are no longer visible.
        if (!ApplicationStatus.hasVisibleActivities()) {
            TtsPlatformImplJni.get().requestTtsStop(
                    mNativeTtsPlatformImplAndroid, TtsPlatformImpl.this);
        }
    }

    @NativeMethods
    interface Natives {
        void requestTtsStop(long nativeTtsPlatformImplAndroid, TtsPlatformImpl caller);
        void voicesChanged(long nativeTtsPlatformImplAndroid, TtsPlatformImpl caller);
        void onEndEvent(long nativeTtsPlatformImplAndroid, TtsPlatformImpl caller, int utteranceId);
        void onStartEvent(
                long nativeTtsPlatformImplAndroid, TtsPlatformImpl caller, int utteranceId);
        void onErrorEvent(
                long nativeTtsPlatformImplAndroid, TtsPlatformImpl caller, int utteranceId);
    }
}
