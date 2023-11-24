// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.util.Size;

/**
 * This class is used as a means to guess the actual screen resolution that the
 * device is capable of playing.
 */
public class ScreenResolutionUtil {
    public static boolean isResolutionSupportedForType(String mimeType, Size targetResolution) {
        MediaCodecInfo[] codecInfos = new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos();
        for (MediaCodecInfo codecInfo : codecInfos) {
            try {
                MediaCodecInfo.CodecCapabilities codecCapabilities =
                        codecInfo.getCapabilitiesForType(mimeType);
                if (codecCapabilities == null) {
                    continue;
                }
                MediaCodecInfo.VideoCapabilities videoCapabilities =
                        codecCapabilities.getVideoCapabilities();
                if (videoCapabilities == null) {
                    continue;
                }
                if (videoCapabilities.isSizeSupported(
                        targetResolution.getWidth(), targetResolution.getHeight())) {
                    return true;
                }
            } catch (IllegalArgumentException e) {
                continue;
            }
        }
        return false;
    }
}
