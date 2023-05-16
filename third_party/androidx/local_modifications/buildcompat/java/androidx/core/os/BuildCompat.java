/*
 * Copyright (C) 2016 The Android Open Source Project
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

package androidx.core.os;

import static android.os.ext.SdkExtensions.getExtensionVersion;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.ext.SdkExtensions;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresOptIn;
import androidx.annotation.VisibleForTesting;

import java.util.Locale;

/**
 * This class contains additional platform version checking methods for targeting pre-release
 * versions of Android.
 */
public class BuildCompat {

    private BuildCompat() {
        // This class is non-instantiable.
    }

    /**
     * Checks if the codename is a matching or higher version than the given build value.
     * @param codename the requested build codename, e.g. {@code "O"} or {@code "OMR1"}
     * @param buildCodename the value of {@link Build.VERSION#CODENAME}
     *
     * @return {@code true} if APIs from the requested codename are available in the build.
     *
     * @hide
     */
    @VisibleForTesting
    protected static boolean isAtLeastPreReleaseCodename(@NonNull String codename,
            @NonNull String buildCodename) {

        // Special case "REL", which means the build is not a pre-release build.
        if ("REL".equals(buildCodename)) {
            return false;
        }

        // Otherwise lexically compare them.  Return true if the build codename is equal to or
        // greater than the requested codename.
        final String buildCodenameUpper = buildCodename.toUpperCase(Locale.ROOT);
        final String codenameUpper = codename.toUpperCase(Locale.ROOT);
        return buildCodenameUpper.compareTo(codenameUpper) >= 0;
    }

    /**
     * Checks if the device is running on the Android N release or newer.
     *
     * @return {@code true} if N APIs are available for use
     * @deprecated Android N is a finalized release and this method is no longer necessary. It will
     *             be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.N}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.N)
    @Deprecated
    public static boolean isAtLeastN() {
        return VERSION.SDK_INT >= 24;
    }

    /**
     * Checks if the device is running on the Android N MR1 release or newer.
     *
     * @return {@code true} if N MR1 APIs are available for use
     * @deprecated Android N MR1 is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.N_MR1)
    @Deprecated
    public static boolean isAtLeastNMR1() {
        return VERSION.SDK_INT >= 25;
    }

    /**
     * Checks if the device is running on a release version of Android O or newer.
     * <p>
     * @return {@code true} if O APIs are available for use, {@code false} otherwise
     * @deprecated Android O is a finalized release and this method is no longer necessary. It will
     *             be removed in a future release of the Support Library. Instead use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.O}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.O)
    @Deprecated
    public static boolean isAtLeastO() {
        return VERSION.SDK_INT >= 26;
    }

    /**
     * Checks if the device is running on a release version of Android O MR1 or newer.
     * <p>
     * @return {@code true} if O MR1 APIs are available for use, {@code false} otherwise
     * @deprecated Android O MR1 is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.O_MR1)
    @Deprecated
    public static boolean isAtLeastOMR1() {
        return VERSION.SDK_INT >= 27;
    }

    /**
     * Checks if the device is running on a release version of Android P or newer.
     * <p>
     * @return {@code true} if P APIs are available for use, {@code false} otherwise
     * @deprecated Android P is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.P}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.P)
    @Deprecated
    public static boolean isAtLeastP() {
        return VERSION.SDK_INT >= 28;
    }

    /**
     * Checks if the device is running on release version of Android Q or newer.
     * <p>
     * @return {@code true} if Q APIs are available for use, {@code false} otherwise
     * @deprecated Android Q is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.Q)
    @Deprecated
    public static boolean isAtLeastQ() {
        return VERSION.SDK_INT >= 29;
    }

    /**
     * Checks if the device is running on release version of Android R or newer.
     * <p>
     * @return {@code true} if R APIs are available for use, {@code false} otherwise
     * @deprecated Android R is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of the Support Library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.R}.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.R)
    @Deprecated
    public static boolean isAtLeastR() {
        return VERSION.SDK_INT >= 30;
    }

    /**
     * Checks if the device is running on a pre-release version of Android S or a release version of
     * Android S or newer.
     *
     * @return {@code true} if S APIs are available for use, {@code false} otherwise
     * @deprecated Android S is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of this library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= 31}.
     */
    @SuppressLint("RestrictedApi")
    @ChecksSdkIntAtLeast(api = 31, codename = "S")
    @Deprecated
    public static boolean isAtLeastS() {
        return VERSION.SDK_INT >= 31
                || (VERSION.SDK_INT >= 30 && isAtLeastPreReleaseCodename("S", VERSION.CODENAME));
    }

    /**
     * Checks if the device is running on a pre-release version of Android Sv2 or a release
     * version of Android Sv2 or newer.
     *
     * @return {@code true} if Sv2 APIs are available for use, {@code false} otherwise
     * @deprecated Android Sv2 is a finalized release and this method is no longer necessary. It
     *             will be removed in a future release of this library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= 32}.
     */
    @PrereleaseSdkCheck
    @ChecksSdkIntAtLeast(api = 32, codename = "Sv2")
    @Deprecated
    public static boolean isAtLeastSv2() {
        return VERSION.SDK_INT >= 32
                || (VERSION.SDK_INT >= 31
                && isAtLeastPreReleaseCodename("Sv2", VERSION.CODENAME));
    }

    /**
     * Checks if the device is running on a pre-release version of Android Tiramisu or a release
     * version of Android Tiramisu or newer.
     * <p>
     * <strong>Note:</strong> When Android Tiramisu is finalized for release, this method will be
     * removed and all calls must be replaced with {@code Build.VERSION.SDK_INT >= 33}.
     *
     * @return {@code true} if Tiramisu APIs are available for use, {@code false} otherwise
     * @deprecated Android Tiramisu is a finalized release and this method is no longer necessary.
     *             It will be removed in a future release of this library. Instead, use
     *             {@code Build.VERSION.SDK_INT >= 33}.
     */
    @PrereleaseSdkCheck
    @ChecksSdkIntAtLeast(api = 33, codename = "Tiramisu")
    @Deprecated
    public static boolean isAtLeastT() {
        return VERSION.SDK_INT >= 33
                || (VERSION.SDK_INT >= 32
                && isAtLeastPreReleaseCodename("Tiramisu", VERSION.CODENAME));
    }

    /**
     * Checks if the device is running on a pre-release version of Android UpsideDownCake or a
     * release version of Android UpsideDownCake or newer.
     * <p>
     * <strong>Note:</strong> When Android UpsideDownCake is finalized for release, this method
     * will be removed and all calls must be replaced with {@code Build.VERSION.SDK_INT >= 34}.
     *
     * @return {@code true} if UpsideDownCake APIs are available for use, {@code false} otherwise
     */
    @PrereleaseSdkCheck
    @ChecksSdkIntAtLeast(api = 34, codename = "UpsideDownCake")
    public static boolean isAtLeastU() {
        return VERSION.SDK_INT >= 34
                || (VERSION.SDK_INT >= 33
                && isAtLeastPreReleaseCodename("UpsideDownCake", VERSION.CODENAME));
    }

    /**
     * Checks if the device is running on a pre-release version of Android VanillaIceCream.
     * <p>
     * <strong>Note:</strong> When Android anillaIceCream is finalized for release, this method will
     * be removed and all calls must be replaced with {@code Build.VERSION.SDK_INT >=
     * Build.VERSION_CODES.VANILLA_ICE_CREAM}.
     *
     * @return {@code true} if VanillaIceCream APIs are available for use, {@code false} otherwise
     */
    @PrereleaseSdkCheck
    @ChecksSdkIntAtLeast(codename = "VanillaIceCream")
    public static boolean isAtLeastV() {
        return VERSION.SDK_INT >= 34
                && isAtLeastPreReleaseCodename("VanillaIceCream", VERSION.CODENAME);
    }

    /**
     * Experimental feature set for pre-release SDK checks.
     * <p>
     * APIs annotated as part of this feature set should only be used when building against
     * pre-release platform SDKs. They are safe to ship in production apps and alpha libraries,
     * but they must not be shipped in beta or later libraries as they <strong>will be
     * removed</strong> after their respective SDKs are finalized for release.
     */
    @RequiresOptIn
    public @interface PrereleaseSdkCheck { }

    /**
     * The value of {@code SdkExtensions.getExtensionVersion(R)}. This is a convenience constant
     * which provides the extension version in a similar style to {@code Build.VERSION.SDK_INT}.
     * <p>
     * Compared to calling {@code getExtensionVersion} directly, using this constant has the
     * benefit of not having to verify the {@code getExtensionVersion} method is available.
     *
     * @return the version of the R extension, if it exists. 0 otherwise.
     */
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.R)
    @SuppressLint("CompileTimeConstant")
    public static final int R_EXTENSION_INT = VERSION.SDK_INT >= 30 ? Extensions30Impl.R : 0;

    /**
     * The value of {@code SdkExtensions.getExtensionVersion(S)}. This is a convenience constant
     * which provides the extension version in a similar style to {@code Build.VERSION.SDK_INT}.
     * <p>
     * Compared to calling {@code getExtensionVersion} directly, using this constant has the
     * benefit of not having to verify the {@code getExtensionVersion} method is available.
     *
     * @return the version of the S extension, if it exists. 0 otherwise.
     */
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.S)
    @SuppressLint("CompileTimeConstant")
    public static final int S_EXTENSION_INT = VERSION.SDK_INT >= 30 ? Extensions30Impl.S : 0;

    /**
     * The value of {@code SdkExtensions.getExtensionVersion(TIRAMISU)}. This is a convenience
     * constant which provides the extension version in a similar style to
     * {@code Build.VERSION.SDK_INT}.
     * <p>
     * Compared to calling {@code getExtensionVersion} directly, using this constant has the
     * benefit of not having to verify the {@code getExtensionVersion} method is available.
     *
     * @return the version of the T extension, if it exists. 0 otherwise.
     */
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.TIRAMISU)
    @SuppressLint("CompileTimeConstant")
    public static final int T_EXTENSION_INT = VERSION.SDK_INT >= 30 ? Extensions30Impl.TIRAMISU : 0;

    /**
     * constant which provides the extension version in a similar style to
     * {@code Build.VERSION.SDK_INT}.
     * <p>
     * Compared to calling {@code getExtensionVersion} directly, using this constant has the
     * benefit of not having to verify the {@code getExtensionVersion} method is available.
     *
     * @return the version of the AdServices extension, if it exists. 0 otherwise.
     */
    @SuppressLint("CompileTimeConstant")

    @RequiresApi(30)
    private static final class Extensions30Impl {
        static final int R = getExtensionVersion(Build.VERSION_CODES.R);
        static final int S = getExtensionVersion(Build.VERSION_CODES.S);
        static final int TIRAMISU = getExtensionVersion(Build.VERSION_CODES.TIRAMISU);
    }

}
