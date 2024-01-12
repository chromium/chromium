/*
 * Copyright 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.window.extensions.area;

import android.app.Activity;
import android.util.DisplayMetrics;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Consumer;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * The interface definition that will be used by the WindowManager library to get custom
 * OEM-provided behavior around moving windows between displays or display areas on a device.
 *
 * Currently the only behavior supported is RearDisplay Mode, where the window
 * is moved to the display that faces the same direction as the rear camera.
 *
 * <p>This interface should be implemented by OEM and deployed to the target devices.
 * @see WindowExtensions#getWindowLayoutComponent()
 */
public interface WindowAreaComponent {

    /**
     * WindowArea status constant to signify that the feature is
     * unsupported on this device. Could be due to the device not supporting that
     * specific feature.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    int STATUS_UNSUPPORTED = 0;

    /**
     * WindowArea status constant to signify that the feature is
     * currently unavailable but is supported on this device. This value could signify
     * that the current device state does not support the specific feature or another
     * process is currently enabled in that feature.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    int STATUS_UNAVAILABLE = 1;

    /**
     * WindowArea status constant to signify that the feature is
     * available to be entered or enabled.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    int STATUS_AVAILABLE = 2;

    /**
     * WindowArea status constant to signify that the feature is
     * already enabled.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    int STATUS_ACTIVE = 3;

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_PARAMETER, ElementType.TYPE_USE})
    @IntDef({
            STATUS_UNSUPPORTED,
            STATUS_UNAVAILABLE,
            STATUS_AVAILABLE,
            STATUS_ACTIVE
    })
    @interface WindowAreaStatus {}

    /**
     * Session state constant to represent there being no active session
     * currently in progress. Used by the library to call the correct callbacks if
     * a session is ended.
     */
    int SESSION_STATE_INACTIVE = 0;

    /**
     * Session state constant to represent that there is currently an active session. The library
     * uses this state to know when a session is created and active. Note that this state is
     * different from SESSION_STATE_CONTENT_VISIBLE, because the presentation content in this state
     * is not visible.
     */
    int SESSION_STATE_ACTIVE = 1;

    /**
     * Session state constant to represent that there is an
     * active presentation session currently in progress, and the content provided by the
     * application is visible.
     */
    int SESSION_STATE_CONTENT_VISIBLE = 2;

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_PARAMETER, ElementType.TYPE_USE})
    @IntDef({
            SESSION_STATE_ACTIVE,
            SESSION_STATE_INACTIVE,
            SESSION_STATE_CONTENT_VISIBLE
    })
    @interface WindowAreaSessionState {}

    /**
     * Adds a listener interested in receiving updates on the RearDisplayStatus
     * of the device. Because this is being called from the OEM provided
     * extensions, the library will post the result of the listener on the executor
     * provided by the developer.
     *
     * The listener provided will receive values that
     * correspond to the [WindowAreaStatus] value that aligns with the current status
     * of the rear display.
     * @param consumer interested in receiving updates to WindowAreaStatus.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    void addRearDisplayStatusListener(@NonNull Consumer<Integer> consumer);

    /**
     * Removes a listener no longer interested in receiving updates.
     * @param consumer no longer interested in receiving updates to WindowAreaStatus
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    void removeRearDisplayStatusListener(@NonNull Consumer<Integer> consumer);

    /**
     * Creates and starts a rear display session and sends state updates to the
     * consumer provided. This consumer will receive a constant represented by
     * [WindowAreaSessionState] to represent the state of the current rear display
     * session. We will translate to a more friendly interface in the library.
     *
     * Because this is being called from the OEM provided extensions, the library
     * will post the result of the listener on the executor provided by the developer.
     *
     * @param activity to allow that the OEM implementation will use as a base
     * context and to identify the source display area of the request.
     * The reference to the activity instance must not be stored in the OEM
     * implementation to prevent memory leaks.
     * @param consumer to provide updates to the client on the status of the session
     * @throws UnsupportedOperationException if this method is called when RearDisplay
     * mode is not available. This could be to an incompatible device state or when
     * another process is currently in this mode.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    @SuppressWarnings("ExecutorRegistration") // Jetpack will post it on the app-provided executor.
    void startRearDisplaySession(@NonNull Activity activity,
            @NonNull Consumer<@WindowAreaSessionState Integer> consumer);

    /**
     * Ends a RearDisplaySession and sends [STATE_INACTIVE] to the consumer
     * provided in the {@code startRearDisplaySession} method. This method is only
     * called through the {@code RearDisplaySession} provided to the developer.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    void endRearDisplaySession();

    /**
     * Adds a listener interested in receiving updates on the rear display presentation status
     * of the device. Because this is being called from the OEM provided
     * extensions, the library will post the result of the listener on the executor
     * provided by the developer.
     *
     * The listener provided will receive {@link ExtensionWindowAreaStatus} values that
     * correspond to the current status of the feature.
     *
     * @param consumer interested in receiving updates to {@link ExtensionWindowAreaStatus}.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    default void addRearDisplayPresentationStatusListener(
            @NonNull Consumer<ExtensionWindowAreaStatus> consumer) {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Removes a listener no longer interested in receiving updates.
     *
     * @param consumer no longer interested in receiving updates to WindowAreaStatus
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    default void removeRearDisplayPresentationStatusListener(
            @NonNull Consumer<ExtensionWindowAreaStatus> consumer) {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Creates and starts a rear display presentation session and sends state updates to the
     * consumer provided. This consumer will receive a constant represented by
     * {@link WindowAreaSessionState} to represent the state of the current rear display
     * session. We will translate to a more friendly interface in the library.
     *
     * Because this is being called from the OEM provided extensions, the library
     * will post the result of the listener on the executor provided by the developer.
     *
     * Rear display presentation mode refers to a feature where an {@link Activity} can present
     * additional content on a device with a second display that is facing the same direction
     * as the rear camera (i.e. the cover display on a fold-in style device). The calling
     * {@link Activity} stays on the user-facing display.
     *
     * @param activity that the OEM implementation will use as a base
     * context and to identify the source display area of the request.
     * The reference to the activity instance must not be stored in the OEM
     * implementation to prevent memory leaks.
     * @param consumer to provide updates to the client on the status of the session
     * @throws UnsupportedOperationException if this method is called when rear display presentation
     * mode is not available. This could be to an incompatible device state or when
     * another process is currently in this mode.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    default void startRearDisplayPresentationSession(@NonNull Activity activity,
            @NonNull Consumer<@WindowAreaSessionState Integer> consumer) {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Ends the current rear display presentation session and provides updates to the
     * callback provided. When this is ended, the presented content from the calling
     * {@link Activity} will also be removed from the rear facing display.
     * Because this is being called from the OEM provided extensions, the result of the listener
     * will be posted on the executor provided by the developer at the initial call site.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    default void endRearDisplayPresentationSession() {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Returns the {@link ExtensionWindowAreaPresentation} connected to the active
     * rear display presentation session. If there is no session currently active, then it will
     * return null.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    @Nullable
    default ExtensionWindowAreaPresentation getRearDisplayPresentation() {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Returns the {@link android.util.DisplayMetrics} associated with the rear facing display. If
     * there is no rear facing display available on the device, returns an empty
     * {@link android.util.DisplayMetrics} object.
     *
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    // TODO(b/273807238): Investigate how we can provide a listener to get runtime changes in
    //  rear display metrics to better support other form-factors in the future.
    @NonNull
    default DisplayMetrics getRearDisplayMetrics() {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }
}
