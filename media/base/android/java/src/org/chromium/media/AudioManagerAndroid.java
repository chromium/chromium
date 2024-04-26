// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.audiofx.AcousticEchoCanceler;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils.ThreadChecker;

import java.lang.reflect.Method;
import java.util.Optional;

@JNINamespace("media")
class AudioManagerAndroid {
    private static final String TAG = "media";

    // Set to true to enable debug logs. Avoid in production builds.
    // NOTE: always check in as false.
    private static final boolean DEBUG = false;

    /** Simple container for device information. */
    public static class AudioDeviceName {
        private final int mId;
        private final String mName;

        public AudioDeviceName(int id, String name) {
            mId = id;
            mName = name;
        }

        @CalledByNative("AudioDeviceName")
        private String id() {
            return String.valueOf(mId);
        }

        @CalledByNative("AudioDeviceName")
        private String name() {
            return mName;
        }
    }

    // Use 44.1kHz as the default sampling rate.
    private static final int DEFAULT_SAMPLING_RATE = 44100;
    // Randomly picked up frame size which is close to return value on N4.
    // Return this value when getProperty(PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
    // fails.
    private static final int DEFAULT_FRAME_PER_BUFFER = 256;

    private final AudioManager mAudioManager;
    private final long mNativeAudioManagerAndroid;

    // Enabled during initialization if MODIFY_AUDIO_SETTINGS permission is
    // granted. Required to shift system-wide audio settings.
    private boolean mHasModifyAudioSettingsPermission;

    private boolean mIsInitialized;
    private boolean mSavedIsSpeakerphoneOn;
    private boolean mSavedIsMicrophoneMute;

    // This class should be created, initialized and closed on the audio thread
    // in the audio manager. We use |mThreadChecker| to ensure that this is
    // the case.
    private final ThreadChecker mThreadChecker = new ThreadChecker();

    private final ContentResolver mContentResolver;
    private ContentObserver mSettingsObserver;
    private HandlerThread mSettingsObserverThread;

    private AudioDeviceSelector mAudioDeviceSelector;

    /** Construction */
    @CalledByNative
    private static AudioManagerAndroid createAudioManagerAndroid(long nativeAudioManagerAndroid) {
        return new AudioManagerAndroid(nativeAudioManagerAndroid);
    }

    private AudioManagerAndroid(long nativeAudioManagerAndroid) {
        mNativeAudioManagerAndroid = nativeAudioManagerAndroid;
        mAudioManager =
                (AudioManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        mContentResolver = ContextUtils.getApplicationContext().getContentResolver();

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            mAudioDeviceSelector = new AudioDeviceSelectorPreS(mAudioManager);
        } else {
            mAudioDeviceSelector = new AudioDeviceSelectorPostS(mAudioManager);
        }
    }

    /**
     * Saves the initial speakerphone and microphone state.
     * Populates the list of available audio devices and registers receivers for broadcasting
     * intents related to wired headset and Bluetooth devices and USB audio devices.
     */
    @CalledByNative
    private void init() {
        mThreadChecker.assertOnValidThread();
        if (DEBUG) logd("init");
        if (DEBUG) logDeviceInfo();
        if (mIsInitialized) return;

        // Check if process has MODIFY_AUDIO_SETTINGS and RECORD_AUDIO
        // permissions. Both are required for full functionality.
        mHasModifyAudioSettingsPermission =
                hasPermission(android.Manifest.permission.MODIFY_AUDIO_SETTINGS);
        if (DEBUG && !mHasModifyAudioSettingsPermission) {
            logd("MODIFY_AUDIO_SETTINGS permission is missing");
        }

        mAudioDeviceSelector.init();

        mIsInitialized = true;
    }

    /**
     * Unregister all previously registered intent receivers and restore
     * the stored state (stored in {@link #init()}).
     */
    @CalledByNative
    private void close() {
        mThreadChecker.assertOnValidThread();
        if (DEBUG) logd("close");
        if (!mIsInitialized) return;

        stopObservingVolumeChanges();

        mAudioDeviceSelector.close();

        mIsInitialized = false;
    }

    /**
     * Sets audio mode as COMMUNICATION if input parameter is true.
     * Restores audio mode to NORMAL if input parameter is false.
     * Required permission: android.Manifest.permission.MODIFY_AUDIO_SETTINGS.
     */
    @CalledByNative
    private void setCommunicationAudioModeOn(boolean on) {
        mThreadChecker.assertOnValidThread();
        if (DEBUG) logd("setCommunicationAudioModeOn" + on + ")");
        if (!mIsInitialized) return;

        // The MODIFY_AUDIO_SETTINGS permission is required to allow an
        // application to modify global audio settings.
        if (!mHasModifyAudioSettingsPermission) {
            Log.w(
                    TAG,
                    "MODIFY_AUDIO_SETTINGS is missing => client will run "
                            + "with reduced functionality");
            return;
        }

        // TODO(crbug.com/40222537): Should we exit early if we are already in/out of
        // communication mode?
        if (on) {
            // Store microphone mute state and speakerphone state so it can
            // be restored when closing.
            mSavedIsSpeakerphoneOn = mAudioDeviceSelector.isSpeakerphoneOn();
            mSavedIsMicrophoneMute = mAudioManager.isMicrophoneMute();

            mAudioDeviceSelector.setCommunicationAudioModeOn(true);

            // Start observing volume changes to detect when the
            // voice/communication stream volume is at its lowest level.
            // It is only possible to pull down the volume slider to about 20%
            // of the absolute minimum (slider at far left) in communication
            // mode but we want to be able to mute it completely.
            startObservingVolumeChanges();
        } else {
            stopObservingVolumeChanges();

            mAudioDeviceSelector.setCommunicationAudioModeOn(false);

            // Restore previously stored audio states.
            setMicrophoneMute(mSavedIsMicrophoneMute);
            mAudioDeviceSelector.setSpeakerphoneOn(mSavedIsSpeakerphoneOn);
        }

        setCommunicationAudioModeOnInternal(on);
    }

    /**
     * Sets audio mode to MODE_IN_COMMUNICATION if input parameter is true.
     * Restores audio mode to MODE_NORMAL if input parameter is false.
     */
    private void setCommunicationAudioModeOnInternal(boolean on) {
        if (DEBUG) logd("setCommunicationAudioModeOn(" + on + ")");

        if (on) {
            try {
                mAudioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
            } catch (SecurityException e) {
                logDeviceInfo();
                throw e;
            }

        } else {
            // Restore the mode that was used before we switched to
            // communication mode.
            try {
                mAudioManager.setMode(AudioManager.MODE_NORMAL);
            } catch (SecurityException e) {
                logDeviceInfo();
                throw e;
            }
        }
    }

    /**
     * Activates, i.e., starts routing audio to, the specified audio device.
     *
     * @param deviceId Unique device ID (integer converted to string)
     * representing the selected device. This string is empty if the so-called
     * default device is requested.
     * Required permissions: android.Manifest.permission.MODIFY_AUDIO_SETTINGS
     * and android.Manifest.permission.RECORD_AUDIO.
     */
    @CalledByNative
    private boolean setDevice(String deviceId) {
        if (DEBUG) logd("setDevice: " + deviceId);
        if (!mIsInitialized) return false;

        boolean hasRecordAudioPermission = hasPermission(android.Manifest.permission.RECORD_AUDIO);
        if (!mHasModifyAudioSettingsPermission || !hasRecordAudioPermission) {
            Log.w(
                    TAG,
                    "Requires MODIFY_AUDIO_SETTINGS and RECORD_AUDIO. "
                            + "Selected device will not be available for recording");
            return false;
        }

        return mAudioDeviceSelector.selectDevice(deviceId);
    }

    /**
     * @return the current list of available audio devices.
     * Note that this call does not trigger any update of the list of devices,
     * it only copies the current state in to the output array.
     * Required permissions: android.Manifest.permission.MODIFY_AUDIO_SETTINGS
     * and android.Manifest.permission.RECORD_AUDIO.
     */
    @CalledByNative
    private AudioDeviceName[] getAudioInputDeviceNames() {
        if (DEBUG) logd("getAudioInputDeviceNames");
        if (!mIsInitialized) return null;

        boolean hasRecordAudioPermission = hasPermission(android.Manifest.permission.RECORD_AUDIO);
        if (!mHasModifyAudioSettingsPermission || !hasRecordAudioPermission) {
            Log.w(
                    TAG,
                    "Requires MODIFY_AUDIO_SETTINGS and RECORD_AUDIO. "
                            + "No audio device will be available for recording");
            return null;
        }

        return mAudioDeviceSelector.getAudioInputDeviceNames();
    }

    @CalledByNative
    private int getNativeOutputSampleRate() {
        String sampleRateString =
                mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        return sampleRateString == null
                ? DEFAULT_SAMPLING_RATE
                : Integer.parseInt(sampleRateString);
    }

    /**
     * Returns the minimum frame size required for audio input.
     *
     * @param sampleRate sampling rate
     * @param channels number of channels
     */
    @CalledByNative
    private static int getMinInputFrameSize(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_IN_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_IN_STEREO;
        } else {
            return -1;
        }
        return AudioRecord.getMinBufferSize(
                        sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
                / 2
                / channels;
    }

    /**
     * Returns the minimum frame size required for audio output.
     *
     * @param sampleRate sampling rate
     * @param channels number of channels
     */
    @CalledByNative
    private static int getMinOutputFrameSize(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            return -1;
        }
        return AudioTrack.getMinBufferSize(
                        sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
                / 2
                / channels;
    }

    @CalledByNative
    private boolean isAudioLowLatencySupported() {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
    }

    @CalledByNative
    private int getAudioLowLatencyOutputFrameSize() {
        String framesPerBuffer =
                mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        return framesPerBuffer == null
                ? DEFAULT_FRAME_PER_BUFFER
                : Integer.parseInt(framesPerBuffer);
    }

    @CalledByNative
    private static boolean acousticEchoCancelerIsAvailable() {
        return AcousticEchoCanceler.isAvailable();
    }

    // Used for reflection of hidden method getOutputLatency.  Will be `null` before reflection, and
    // a (possibly empty) Optional after.
    private static Optional<Method> sGetOutputLatency;

    // Reflect |methodName(int)|, and return it.
    private static final Method reflectMethod(String methodName) {
        try {
            return AudioManager.class.getMethod(methodName, int.class);
        } catch (NoSuchMethodException e) {
            return null;
        }
    }

    // Return the output latency, as reported by AudioManager.  Do not use this,
    // since it is (a) a hidden API call, and (b) documented as being
    // unreliable.  It's here only to adjust for some hardware devices that do
    // not handle latency properly otherwise.
    // See b/80326798 for more information.
    @CalledByNative
    private int getOutputLatency() {
        mThreadChecker.assertOnValidThread();

        if (sGetOutputLatency == null) {
            // It's okay if this assigns `null`; we won't call it, but we also won't try again to
            // reflect it.
            sGetOutputLatency = Optional.ofNullable(reflectMethod("getOutputLatency"));
        }

        int result = 0;
        if (sGetOutputLatency.isPresent()) {
            try {
                result =
                        (Integer)
                                sGetOutputLatency
                                        .get()
                                        .invoke(mAudioManager, AudioManager.STREAM_MUSIC);
            } catch (Exception e) {
                // Ignore.
            }
        }

        return result;
    }

    /** Sets the microphone mute state. */
    private void setMicrophoneMute(boolean on) {
        boolean wasMuted = mAudioManager.isMicrophoneMute();
        if (wasMuted == on) {
            return;
        }
        mAudioManager.setMicrophoneMute(on);
    }

    /** Gets  the current microphone mute state. */
    private boolean isMicrophoneMute() {
        return mAudioManager.isMicrophoneMute();
    }

    /** Checks if the process has as specified permission or not. */
    private boolean hasPermission(String permission) {
        return ContextUtils.getApplicationContext().checkSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Information about the current build, taken from system properties. */
    private void logDeviceInfo() {
        logd(
                "Android SDK: "
                        + Build.VERSION.SDK_INT
                        + ", "
                        + "Release: "
                        + Build.VERSION.RELEASE
                        + ", "
                        + "Brand: "
                        + Build.BRAND
                        + ", "
                        + "Device: "
                        + Build.DEVICE
                        + ", "
                        + "Id: "
                        + Build.ID
                        + ", "
                        + "Hardware: "
                        + Build.HARDWARE
                        + ", "
                        + "Manufacturer: "
                        + Build.MANUFACTURER
                        + ", "
                        + "Model: "
                        + Build.MODEL
                        + ", "
                        + "Product: "
                        + Build.PRODUCT);
    }

    /** Trivial helper method for debug logging */
    private static void logd(String msg) {
        Log.d(TAG, msg);
    }

    /** Trivial helper method for error logging */
    private static void loge(String msg) {
        Log.e(TAG, msg);
    }

    /** Start thread which observes volume changes on the voice stream. */
    private void startObservingVolumeChanges() {
        if (DEBUG) logd("startObservingVolumeChanges");
        if (mSettingsObserverThread != null) return;
        mSettingsObserverThread = new HandlerThread("SettingsObserver");
        mSettingsObserverThread.start();

        mSettingsObserver =
                new ContentObserver(new Handler(mSettingsObserverThread.getLooper())) {
                    @Override
                    public void onChange(boolean selfChange) {
                        if (DEBUG) logd("SettingsObserver.onChange: " + selfChange);
                        super.onChange(selfChange);

                        // Get stream volume for the voice stream and deliver callback if
                        // the volume index is zero. It is not possible to move the volume
                        // slider all the way down in communication mode but the callback
                        // implementation can ensure that the volume is completely muted.
                        int volume = mAudioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
                        if (DEBUG) logd("AudioManagerAndroidJni.get().setMute: " + (volume == 0));
                        AudioManagerAndroidJni.get()
                                .setMute(
                                        mNativeAudioManagerAndroid,
                                        AudioManagerAndroid.this,
                                        (volume == 0));
                    }
                };

        mContentResolver.registerContentObserver(
                Settings.System.CONTENT_URI, true, mSettingsObserver);
    }

    /** Quit observer thread and stop listening for volume changes. */
    private void stopObservingVolumeChanges() {
        if (DEBUG) logd("stopObservingVolumeChanges");
        if (mSettingsObserverThread == null) return;

        mContentResolver.unregisterContentObserver(mSettingsObserver);
        mSettingsObserver = null;

        mSettingsObserverThread.quit();
        try {
            mSettingsObserverThread.join();
        } catch (InterruptedException e) {
            Log.e(TAG, "Thread.join() exception: ", e);
        }
        mSettingsObserverThread = null;
    }

    /** Return the AudioDeviceInfo array as reported by the Android OS. */
    private static AudioDeviceInfo[] getAudioDeviceInfo() {
        AudioManager audioManager =
                (AudioManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        return audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
    }

    /** Returns whether an audio sink device is connected. */
    @CalledByNative
    private static boolean isAudioSinkConnected() {
        for (AudioDeviceInfo deviceInfo : getAudioDeviceInfo()) {
            if (deviceInfo.isSink()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns a bit mask of Audio Formats (C++ AudioParameters::Format enum)
     * supported by all of the sink devices.
     */
    @CalledByNative
    private static int getAudioEncodingFormatsSupported() {
        int intersection_mask = 0; // intersection of multiple device encoding arrays
        boolean first = true;
        for (AudioDeviceInfo deviceInfo : getAudioDeviceInfo()) {
            int[] encodings = deviceInfo.getEncodings();
            if (deviceInfo.isSink() && deviceInfo.getType() == AudioDeviceInfo.TYPE_HDMI) {
                int mask = 0; // bit mask for a single device

                // Map AudioFormat values to C++ media/base/audio_parameters.h Format enum
                for (int i : encodings) {
                    switch (i) {
                        case AudioFormat.ENCODING_PCM_16BIT:
                            mask |= AudioEncodingFormat.PCM_LINEAR;
                            break;
                        case AudioFormat.ENCODING_AC3:
                            mask |= AudioEncodingFormat.BITSTREAM_AC3;
                            break;
                        case AudioFormat.ENCODING_E_AC3:
                            mask |= AudioEncodingFormat.BITSTREAM_EAC3;
                            break;
                        case AudioFormat.ENCODING_DTS:
                            mask |= AudioEncodingFormat.BITSTREAM_DTS;
                            break;
                        case AudioFormat.ENCODING_DTS_HD:
                            mask |= AudioEncodingFormat.BITSTREAM_DTS_HD;
                            break;
                        case AudioFormat.ENCODING_IEC61937:
                            mask |= AudioEncodingFormat.BITSTREAM_IEC61937;
                            break;
                    }
                }

                // Require all devices to support a format
                if (first) {
                    first = false;
                    intersection_mask = mask;
                } else {
                    intersection_mask &= mask;
                }
            }
        }
        return intersection_mask;
    }

    @NativeMethods
    interface Natives {
        void setMute(long nativeAudioManagerAndroid, AudioManagerAndroid caller, boolean muted);
    }
}
