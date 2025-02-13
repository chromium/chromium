/*
 * Copyright 2024 The Android Open Source Project
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

package androidx.pdf.viewer.fragment.insets

import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams

/**
 * A callback that will update bottom margin for provided view as per keyboard visibility.
 *
 * @param view: A view whose bottom margin needs to be updated with keyboard.
 * @param windowManager: An interface to interact with window params, here helps to fetch screen
 *   height.
 * @param pdfContainer: Container view where PDF is hosted.
 * @param dispatchMode: Specifies whether children view should get callback, by default callback
 *   will be propagated.
 */
internal class TranslateInsetsAnimationCallback(
    private val view: View,
    private val windowManager: WindowManager?,
    private val pdfContainer: View?,
    dispatchMode: Int = DISPATCH_MODE_CONTINUE_ON_SUBTREE
) : WindowInsetsAnimationCompat.Callback(dispatchMode) {

    override fun onProgress(
        insets: WindowInsetsCompat,
        runningAnimations: List<WindowInsetsAnimationCompat>
    ): WindowInsetsCompat {
        // onProgress() is called when any of the running animations progress...

        var absoluteContainerBottom = 0
        /*
        Calculate absolute pdfContainer bottom on screen
        This is necessary as our fragment may not span the complete screen
         */
        pdfContainer?.let {
            val containerLocation = IntArray(2)
            pdfContainer.getLocationInWindow(containerLocation)
            absoluteContainerBottom = pdfContainer.height + containerLocation[1]
        }

        // Extract keyboard insets
        val keyboardInsets = insets.getInsets(WindowInsetsCompat.Type.ime())

        /*
        By default the keyboard top should be aligned with container bottom;
        This is same as keyboard is in closed state
         */
        var keyboardTop = absoluteContainerBottom

        // Calculate keyboard top wrt screen height
        windowManager?.let {
            val screenHeight = windowManager.currentWindowMetrics.bounds.height()
            keyboardTop = screenHeight - keyboardInsets.bottom
        }

        // Net margin wrt pdf container bottom
        val margin =
            if (absoluteContainerBottom >= keyboardTop) absoluteContainerBottom - keyboardTop else 0

        // Update bottom margin for view
        view.updateLayoutParams<ViewGroup.MarginLayoutParams> { bottomMargin = margin }

        return insets
    }
}
