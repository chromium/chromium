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

import com.google.common.collect.ImmutableMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

@JNINamespace("media")
@NullMarked
class AudioManagerAndroid {
    private static final String TAG = "media";
    private static final String MISSING_CHANNEL_MASK_HISTOGRAM_PREFIX =
            "Media.Audio.Android.MissingChannelMask";
    private static final String SUPPORTED_CHANNEL_MASK_HISTOGRAM_PREFIX =
            "Media.Audio.Android.SupportedChannelMask";
    // Set to true to enable debug logs. Avoid in production builds.
    // NOTE: always check in as false.
    private static final boolean DEBUG = false;
    // The LAYOUT_MASK_TO_CHANNEL_COUNT of supported conversions from Android AudioFormat to C++
    // media::ChannelLayout, the above is in sync with the values returned by
    // media::ChannelLayoutToChannelCount().
    private static final Map<Integer, Integer> LAYOUT_MASK_TO_CHANNEL_COUNT =
            ImmutableMap.of(
                    ChannelLayout.LAYOUT_MONO, 1, // CHANNEL_LAYOUT_MONO
                    ChannelLayout.LAYOUT_STEREO, 2, // CHANNEL_LAYOUT_STEREO
                    ChannelLayout.LAYOUT_5_1, 6, // CHANNEL_LAYOUT_5_1
                    ChannelLayout.LAYOUT_7_1, 8, // CHANNEL_LAYOUT_7_1
                    ChannelLayout.LAYOUT_6_1, 7, // CHANNEL_LAYOUT_6_1
                    ChannelLayout.LAYOUT_5_1_4_DOWNMIX, 6 // CHANNEL_LAYOUT_5_1_4 (downmixed to 5.1)
                    );

    /** Simple container for device information. */
    public static class AudioDevice {
        private final int mId;
        private final @Nullable String mName;
        private final int mType;

        public AudioDevice(int id, @Nullable String name, int type) {
            mId = id;
            mName = name;
            mType = type;
        }

        @CalledByNative("AudioDevice")
        private int id() {
            return mId;
        }

        @CalledByNative("AudioDevice")
        private @Nullable String name() {
            return mName;
        }

        @CalledByNative("AudioDevice")
        private int type() {
            return mType;
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
    private final boolean mIsAutomotive;
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
    private @Nullable ContentObserver mSettingsObserver;
    private @Nullable HandlerThread mSettingsObserverThread;

    private final CommunicationDeviceSelector mCommunicationDeviceSelector;

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
            mCommunicationDeviceSelector = new CommunicationDeviceSelectorPreS(mAudioManager);
        } else {
            mCommunicationDeviceSelector = new CommunicationDeviceSelectorPostS(mAudioManager);
        }
        mIsAutomotive =
                ContextUtils.getApplicationContext()
                        .getPackageManager()
                        .hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
    }

    /**
     * Populates the list of available communication devices and registers receivers for
     * broadcasting intents related to wired headset and Bluetooth devices and USB audio devices.
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

        mCommunicationDeviceSelector.init();
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

        mCommunicationDeviceSelector.close();

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
            mSavedIsSpeakerphoneOn = mCommunicationDeviceSelector.isSpeakerphoneOn();
            mSavedIsMicrophoneMute = mAudioManager.isMicrophoneMute();

            mCommunicationDeviceSelector.setCommunicationAudioModeOn(true);

            // Start observing volume changes to detect when the
            // voice/communication stream volume is at its lowest level.
            // It is only possible to pull down the volume slider to about 20%
            // of the absolute minimum (slider at far left) in communication
            // mode but we want to be able to mute it completely.
            startObservingVolumeChanges();
        } else {
            stopObservingVolumeChanges();

            mCommunicationDeviceSelector.setCommunicationAudioModeOn(false);

            // Restore previously stored audio states.
            setMicrophoneMute(mSavedIsMicrophoneMute);
            mCommunicationDeviceSelector.setSpeakerphoneOn(mSavedIsSpeakerphoneOn);
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
     * Sets the system communication device, causing audio with communication-related usage to start
     * being routed to the specified device. Required permissions:
     * android.Manifest.permission.MODIFY_AUDIO_SETTINGS and
     * android.Manifest.permission.RECORD_AUDIO.
     *
     * @param deviceId Unique communication device ID (integer converted to string) representing the
     *     selected device. This string is empty if the so-called default device is requested.
     */
    @CalledByNative
    private boolean setCommunicationDevice(String deviceId) {
        if (DEBUG) logd("setCommunicationDevice: " + deviceId);
        if (!mIsInitialized) return false;

        boolean hasRecordAudioPermission = hasPermission(android.Manifest.permission.RECORD_AUDIO);
        if (!mHasModifyAudioSettingsPermission || !hasRecordAudioPermission) {
            Log.w(
                    TAG,
                    "Requires MODIFY_AUDIO_SETTINGS and RECORD_AUDIO. "
                            + "Selected device will not be available for recording");
            return false;
        }

        return mCommunicationDeviceSelector.selectDevice(deviceId);
    }

    /**
     * @param inputs If true, input devices will be returned; otherwise, output devices will be
     *     returned.
     * @return The current list of available audio devices. Note that this call does not trigger any
     *     update of the list of devices, it only copies the current state into the output array.
     */
    @CalledByNative
    private AudioDevice @Nullable [] getDevices(boolean inputs) {
        if (DEBUG) logd("getDevices");

        AudioDeviceInfo[] deviceInfos =
                mAudioManager.getDevices(
                        inputs
                                ? AudioManager.GET_DEVICES_INPUTS
                                : AudioManager.GET_DEVICES_OUTPUTS);

        List<AudioDevice> devices = new ArrayList<>();
        for (int deviceIndex = 0; deviceIndex < deviceInfos.length; deviceIndex++) {
            AudioDeviceInfo deviceInfo = deviceInfos[deviceIndex];

            int type = deviceInfo.getType();
            switch (type) {
                case 28: // AudioDeviceInfo.TYPE_ECHO_REFERENCE
                case AudioDeviceInfo.TYPE_REMOTE_SUBMIX:
                case AudioDeviceInfo.TYPE_TELEPHONY:
                    // Unusable device types.
                    continue;
                case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                    // TODO(crbug.com/405955144): Bluetooth Classic output streams do not work
                    // correctly, as they do not react to SCO state changes.
                    if (!inputs) {
                        continue;
                    }
            }

            int id = deviceInfo.getId();
            String name = deviceInfo.getProductName().toString();
            if (name.equals(Build.MODEL)) {
                // Undo the Android framework's substitution of a missing name with
                // `android.os.Build.MODEL` to facilitate providing a custom fallback name instead.
                name = null;
            }
            devices.add(new AudioDevice(id, name, type));
        }
        return devices.toArray(new AudioDevice[0]);
    }

    /**
     * Required permissions: android.Manifest.permission.MODIFY_AUDIO_SETTINGS and
     * android.Manifest.permission.RECORD_AUDIO.
     *
     * @return The current list of available communication devices. Note that this call does not
     *     trigger any update of the list of devices, it only copies the current state into the
     *     output array.
     */
    @CalledByNative
    private AudioDevice @Nullable [] getCommunicationDevices() {
        if (DEBUG) logd("getCommunicationDevices");
        if (!mIsInitialized) return null;

        boolean hasRecordAudioPermission = hasPermission(android.Manifest.permission.RECORD_AUDIO);
        if (!mHasModifyAudioSettingsPermission || !hasRecordAudioPermission) {
            Log.w(
                    TAG,
                    "Requires MODIFY_AUDIO_SETTINGS and RECORD_AUDIO. "
                            + "No audio device will be available for recording");
            return null;
        }

        return mCommunicationDeviceSelector.getDevices();
    }

    /** Gets whether Bluetooth SCO is currently enabled. */
    @CalledByNative
    private boolean isBluetoothScoOn() {
        return mCommunicationDeviceSelector.isBluetoothScoOn();
    }

    /** Requests for Bluetooth SCO to be enabled or disabled. This request may fail. */
    @CalledByNative
    private void maybeSetBluetoothScoState(boolean state) {
        mCommunicationDeviceSelector.maybeSetBluetoothScoState(state);
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
    @SuppressWarnings("NullableOptional")
    private static @Nullable Optional<Method> sGetOutputLatency;

    // Reflect |methodName(int)|, and return it.
    private static final @Nullable Method reflectMethod(String methodName) {
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

        assert mSettingsObserver != null;
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
     * Returns a bit mask of Audio Formats (C++ AudioParameters::Format enum) supported by all of
     * the sink devices.
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

    /**
     * Retrieves the maximum supported channel layout for automotive audio.
     *
     * This method identifies the channel layout with the highest number of channels among all
     * available audio devices of type {@link AudioDeviceInfo#TYPE_BUS}. It's designed specifically
     * for automotive systems (when {@code mIsAutomotive} is true); otherwise, it returns {@link
     * ChannelLayout#LAYOUT_UNSUPPORTED}.
     *
     * @return The {@link ChannelLayout} with the maximum number of channels supported, or {@link
     *     ChannelLayout#LAYOUT_UNSUPPORTED} if not in automotive mode or no supported devices are
     *     found.
     */
    @CalledByNative
    int getLayoutWithMaxChannels() {
        if (!mIsAutomotive) {
            return ChannelLayout.LAYOUT_UNSUPPORTED;
        }
        // A set is used since different AudioDeviceInfo can have the same channel mask
        Set<Integer> supportedChannelLayoutSet = new HashSet<>();
        Set<Integer> unsupportedChannelLayoutSet = new HashSet<>();
        // The default Channel Layout for Android is Stereo.
        int maxChannelLayout = ChannelLayout.LAYOUT_STEREO;
        int maxChannelCount = 2;

        for (AudioDeviceInfo deviceInfo :
                mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (deviceInfo.getType() != AudioDeviceInfo.TYPE_BUS) {
                continue;
            }
            int[] channelMasks = deviceInfo.getChannelMasks();
            for (int index = 0; index < channelMasks.length; index++) {
                int converted = getChannelLayoutFromChannelMask(channelMasks[index]);

                if (converted != ChannelLayout.LAYOUT_UNSUPPORTED) {
                    int channelCount = LAYOUT_MASK_TO_CHANNEL_COUNT.getOrDefault(converted, 0);
                    // The following logic looks at the number of channels.
                    // For example on system with both 5.1.4 and 7.1, we will choose 7.1
                    // Chromium downmixes 5.1.4 to 5.1, therefore we must keep 7.1 as the best
                    // output.
                    // The logic is tied to the LAYOUT_MASK_TO_CHANNEL_COUNT and C layer.
                    if (channelCount > maxChannelCount) {
                        maxChannelLayout = converted;
                        maxChannelCount = channelCount;
                    }
                    supportedChannelLayoutSet.add(channelMasks[index]);
                } else {
                    unsupportedChannelLayoutSet.add(channelMasks[index]);
                }
            }
        }

        // Record the supported and unsupported channel layout sets.
        recordChannelLayout(MISSING_CHANNEL_MASK_HISTOGRAM_PREFIX, unsupportedChannelLayoutSet);
        recordChannelLayout(SUPPORTED_CHANNEL_MASK_HISTOGRAM_PREFIX, supportedChannelLayoutSet);
        return maxChannelLayout;
    }

    /**
     * Converts an Android AudioFormat channel mask to a Chromium ChannelLayout constant.
     *
     * @param mask The input channel mask from AudioFormat.
     * @return The corresponding ChannelLayout constant, or LAYOUT_UNSUPPORTED if no match is found.
     */
    private static int getChannelLayoutFromChannelMask(int mask) {
        // TODO(crbug.com/415145629): Support 7.1.2 and 7.1.4
        switch (mask) {
            case AudioFormat.CHANNEL_OUT_MONO:
                return ChannelLayout.LAYOUT_MONO;

            case AudioFormat.CHANNEL_OUT_STEREO:
                return ChannelLayout.LAYOUT_STEREO;

            case AudioFormat.CHANNEL_OUT_5POINT1:
                return ChannelLayout.LAYOUT_5_1;

            case AudioFormat.CHANNEL_OUT_5POINT1POINT4:
                return ChannelLayout.LAYOUT_5_1_4_DOWNMIX;

            case AudioFormat.CHANNEL_OUT_6POINT1:
                return ChannelLayout.LAYOUT_6_1;

            case AudioFormat.CHANNEL_OUT_7POINT1_SURROUND:
                return ChannelLayout.LAYOUT_7_1;
        }
        return ChannelLayout.LAYOUT_UNSUPPORTED;
    }

    // Utility function to record sparse histogram.
    private static void recordChannelLayout(String text, Set<Integer> channelLayoutSet) {
        for (Integer channelLayoutMask : channelLayoutSet) {
            RecordHistogram.recordSparseHistogram(text, channelLayoutMask);
        }
    }

    @NativeMethods
    interface Natives {
        void setMute(long nativeAudioManagerAndroid, AudioManagerAndroid caller, boolean muted);
    }
}
