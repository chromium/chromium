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

package androidx.core.os

import android.os.Build
import android.os.ext.SdkExtensions
import androidx.annotation.ChecksSdkIntAtLeast
import androidx.annotation.RequiresApi
import androidx.annotation.RestrictTo
import androidx.annotation.VisibleForTesting

/**
 * This class contains additional platform version checking methods for targeting pre-release
 * versions of Android.
 */
public object BuildCompat {

    /**
     * Checks if the codename is a matching or higher version than the given build value.
     *
     * @param codename the requested build codename, e.g. `"O"` or `"OMR1"`
     * @param buildCodename the value of [Build.VERSION.CODENAME]
     * @return `true` if APIs from the requested codename are available in the build.
     */
    @JvmStatic
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @VisibleForTesting
    public fun isAtLeastPreReleaseCodename(codename: String, buildCodename: String): Boolean {
        // Special case "REL", which means the build is not a pre-release build.
        if ("REL" == buildCodename) {
            return false
        }
        // Otherwise lexically compare them.  Return true if the build codename is equal to or
        // greater than the requested codename.
        return buildCodename.uppercase() >= codename.uppercase()
    }

    /**
     * Checks if the device is running on the Android N release or newer.
     *
     * @return `true` if N APIs are available for use
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.N)
    @Deprecated(
        message =
            "Android N is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 24`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 24")
    )
    public fun isAtLeastN(): Boolean = Build.VERSION.SDK_INT >= 24

    /**
     * Checks if the device is running on the Android N MR1 release or newer.
     *
     * @return `true` if N MR1 APIs are available for use
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.N_MR1)
    @Deprecated(
        message =
            "Android N MR1 is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 25`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 25")
    )
    public fun isAtLeastNMR1(): Boolean = Build.VERSION.SDK_INT >= 25

    /**
     * Checks if the device is running on a release version of Android O or newer.
     *
     * @return `true` if O APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.O)
    @Deprecated(
        message =
            "Android O is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead use " +
                "`Build.VERSION.SDK_INT >= 26`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 26")
    )
    public fun isAtLeastO(): Boolean = Build.VERSION.SDK_INT >= 26

    /**
     * Checks if the device is running on a release version of Android O MR1 or newer.
     *
     * @return `true` if O MR1 APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.O_MR1)
    @Deprecated(
        message =
            "Android O MR1 is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 27`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 27")
    )
    public fun isAtLeastOMR1(): Boolean = Build.VERSION.SDK_INT >= 27

    /**
     * Checks if the device is running on a release version of Android P or newer.
     *
     * @return `true` if P APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.P)
    @Deprecated(
        message =
            "Android P is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 28`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 28")
    )
    public fun isAtLeastP(): Boolean = Build.VERSION.SDK_INT >= 28

    /**
     * Checks if the device is running on release version of Android Q or newer.
     *
     * @return `true` if Q APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.Q)
    @Deprecated(
        message =
            "Android Q is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 29`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 29")
    )
    public fun isAtLeastQ(): Boolean = Build.VERSION.SDK_INT >= 29

    /**
     * Checks if the device is running on release version of Android R or newer.
     *
     * @return `true` if R APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.R)
    @Deprecated(
        message =
            "Android R is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 30`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 30")
    )
    public fun isAtLeastR(): Boolean = Build.VERSION.SDK_INT >= 30

    /**
     * Checks if the device is running on a pre-release version of Android S or a release version of
     * Android S or newer.
     *
     * @return `true` if S APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = 31, codename = "S")
    @Deprecated(
        message =
            "Android S is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 31`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 31")
    )
    public fun isAtLeastS(): Boolean =
        Build.VERSION.SDK_INT >= 31 ||
            (Build.VERSION.SDK_INT >= 30 &&
                isAtLeastPreReleaseCodename("S", Build.VERSION.CODENAME))

    /**
     * Checks if the device is running on a pre-release version of Android Sv2 or a release version
     * of Android Sv2 or newer.
     *
     * @return `true` if Sv2 APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = 32, codename = "Sv2")
    @Deprecated(
        message =
            "Android Sv2 is a finalized release and this method is no longer necessary. " +
                "It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 32`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 32")
    )
    public fun isAtLeastSv2(): Boolean =
        Build.VERSION.SDK_INT >= 32 ||
            (Build.VERSION.SDK_INT >= 31 &&
                isAtLeastPreReleaseCodename("Sv2", Build.VERSION.CODENAME))

    /**
     * Checks if the device is running on a pre-release version of Android Tiramisu or a release
     * version of Android Tiramisu or newer.
     *
     * **Note:** When Android Tiramisu is finalized for release, this method will be removed and all
     * calls must be replaced with `Build.VERSION.SDK_INT >= 33`.
     *
     * @return `true` if Tiramisu APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = 33, codename = "Tiramisu")
    @Deprecated(
        message =
            "Android Tiramisu is a finalized release and this method is no longer " +
                "necessary. It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 33`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 33")
    )
    public fun isAtLeastT(): Boolean =
        Build.VERSION.SDK_INT >= 33 ||
            (Build.VERSION.SDK_INT >= 32 &&
                isAtLeastPreReleaseCodename("Tiramisu", Build.VERSION.CODENAME))

    /**
     * Checks if the device is running on a pre-release version of Android UpsideDownCake or a
     * release version of Android UpsideDownCake or newer.
     *
     * **Note:** When Android UpsideDownCake is finalized for release, this method will be removed
     * and all calls must be replaced with `Build.VERSION.SDK_INT >= 34`.
     *
     * @return `true` if UpsideDownCake APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = 34, codename = "UpsideDownCake")
    @Deprecated(
        message =
            "Android UpsideDownCase is a finalized release and this method is no longer " +
                "necessary. It will be removed in a future release of this library. Instead, use " +
                "`Build.VERSION.SDK_INT >= 34`.",
        ReplaceWith("android.os.Build.VERSION.SDK_INT >= 34")
    )
    public fun isAtLeastU(): Boolean =
        Build.VERSION.SDK_INT >= 34 ||
            (Build.VERSION.SDK_INT >= 33 &&
                isAtLeastPreReleaseCodename("UpsideDownCake", Build.VERSION.CODENAME))

    /**
     * Checks if the device is running on a pre-release version of Android VanillaIceCream or a
     * release version of Android VanillaIceCream or newer.
     *
     * **Note:** When Android VanillaIceCream is finalized for release, this method will be removed
     * and all calls must be replaced with `Build.VERSION.SDK_INT >= 35`.
     *
     * @return `true` if VanillaIceCream APIs are available for use, `false` otherwise
     */
    @JvmStatic
    @ChecksSdkIntAtLeast(api = 35, codename = "VanillaIceCream")
    public fun isAtLeastV(): Boolean =
        Build.VERSION.SDK_INT >= 35 ||
            (Build.VERSION.SDK_INT >= 34 &&
                isAtLeastPreReleaseCodename("VanillaIceCream", Build.VERSION.CODENAME))

    /**
     * Experimental feature set for pre-release SDK checks.
     *
     * Pre-release SDK checks **do not** guarantee correctness, as APIs may have been added or
     * removed during the course of a pre-release SDK development cycle.
     *
     * Additionally, pre-release checks **may not** return `true` when run on a finalized version of
     * the SDK associated with the codename.
     */
    @RequiresOptIn
    @Retention(AnnotationRetention.BINARY)
    public annotation class PrereleaseSdkCheck

    /**
     * The value of `SdkExtensions.getExtensionVersion(R)`. This is a convenience constant which
     * provides the extension version in a similar style to `Build.VERSION.SDK_INT`.
     *
     * Compared to calling `getExtensionVersion` directly, using this constant has the benefit of
     * not having to verify the `getExtensionVersion` method is available.
     *
     * @return the version of the R extension, if it exists. 0 otherwise.
     */
    @JvmField
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.R)
    public val R_EXTENSION_INT: Int =
        if (Build.VERSION.SDK_INT >= 30) {
            Api30Impl.getExtensionVersion(Build.VERSION_CODES.R)
        } else 0

    /**
     * The value of `SdkExtensions.getExtensionVersion(S)`. This is a convenience constant which
     * provides the extension version in a similar style to `Build.VERSION.SDK_INT`.
     *
     * Compared to calling `getExtensionVersion` directly, using this constant has the benefit of
     * not having to verify the `getExtensionVersion` method is available.
     *
     * @return the version of the S extension, if it exists. 0 otherwise.
     */
    @JvmField
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.S)
    public val S_EXTENSION_INT: Int =
        if (Build.VERSION.SDK_INT >= 30) {
            Api30Impl.getExtensionVersion(Build.VERSION_CODES.S)
        } else 0

    /**
     * The value of `SdkExtensions.getExtensionVersion(TIRAMISU)`. This is a convenience constant
     * which provides the extension version in a similar style to `Build.VERSION.SDK_INT`.
     *
     * Compared to calling `getExtensionVersion` directly, using this constant has the benefit of
     * not having to verify the `getExtensionVersion` method is available.
     *
     * @return the version of the T extension, if it exists. 0 otherwise.
     */
    @JvmField
    @ChecksSdkIntAtLeast(extension = Build.VERSION_CODES.TIRAMISU)
    public val T_EXTENSION_INT: Int =
        if (Build.VERSION.SDK_INT >= 30) {
            Api30Impl.getExtensionVersion(Build.VERSION_CODES.TIRAMISU)
        } else 0

    /**
     * The value of `SdkExtensions.getExtensionVersion(AD_SERVICES)`. This is a convenience constant
     * which provides the extension version in a similar style to `Build.VERSION.SDK_INT`.
     *
     * Compared to calling `getExtensionVersion` directly, using this constant has the benefit of
     * not having to verify the `getExtensionVersion` method is available.
     *
     * @return the version of the AdServices extension, if it exists. 0 otherwise.
     */
    @JvmField
    @ChecksSdkIntAtLeast(extension = SdkExtensions.AD_SERVICES)
    public val AD_SERVICES_EXTENSION_INT: Int =
        if (Build.VERSION.SDK_INT >= 30) {
            Api30Impl.getExtensionVersion(SdkExtensions.AD_SERVICES)
        } else 0

    @RequiresApi(30)
    private object Api30Impl {

        fun getExtensionVersion(extension: Int): Int {
            return SdkExtensions.getExtensionVersion(extension)
        }
    }
}
