// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodecInfo.CodecProfileLevel;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.ArrayList;
import java.util.List;

@JNINamespace("media")
class CodecProfileLevelList {
    private static final String TAG = "CodecProfileLevelList";

    private final List<CodecProfileLevelAdapter> mList;

    public CodecProfileLevelList() {
        mList = new ArrayList<CodecProfileLevelAdapter>();
    }

    public boolean addCodecProfileLevel(String mime, CodecProfileLevel codecProfileLevel) {
        try {
            int codec = getCodecFromMime(mime);
            mList.add(
                    new CodecProfileLevelAdapter(
                            codec,
                            mediaCodecProfileToChromiumMediaProfile(
                                    codec, codecProfileLevel.profile),
                            mediaCodecLevelToChromiumMediaLevel(codec, codecProfileLevel.level)));
            return true;
        } catch (UnsupportedCodecProfileException e) {
            return false;
        }
    }

    public boolean addCodecProfileLevel(int codec, int profile, int level) {
        mList.add(new CodecProfileLevelAdapter(codec, profile, level));
        return true;
    }

    public Object[] toArray() {
        return mList.toArray();
    }

    static class CodecProfileLevelAdapter {
        private final int mCodec;
        private final int mProfile;
        private final int mLevel;

        public CodecProfileLevelAdapter(int codec, int profile, int level) {
            mCodec = codec;
            mProfile = profile;
            mLevel = level;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public int getCodec() {
            return mCodec;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public int getProfile() {
            return mProfile;
        }

        @CalledByNative("CodecProfileLevelAdapter")
        public int getLevel() {
            return mLevel;
        }
    }

    private static class UnsupportedCodecProfileException extends RuntimeException {}

    public static int getCodecFromMime(String mime) {
        if (mime.endsWith("vp9")) return VideoCodec.VP9;
        if (mime.endsWith("vp8")) return VideoCodec.VP8;
        if (mime.endsWith("av01")) return VideoCodec.AV1;
        if (mime.endsWith("avc")) return VideoCodec.H264;
        if (mime.endsWith("hevc")) return VideoCodec.HEVC;
        if (mime.endsWith("dolby-vision")) return VideoCodec.DOLBY_VISION;
        throw new UnsupportedCodecProfileException();
    }

    public static int mediaCodecProfileToChromiumMediaProfile(int codec, int profile) {
        switch (codec) {
            case VideoCodec.H264:
                switch (profile) {
                    case CodecProfileLevel.AVCProfileBaseline:
                    case CodecProfileLevel.AVCProfileConstrainedBaseline:
                        return VideoCodecProfile.H264PROFILE_BASELINE;
                    case CodecProfileLevel.AVCProfileMain:
                        return VideoCodecProfile.H264PROFILE_MAIN;
                    case CodecProfileLevel.AVCProfileExtended:
                        return VideoCodecProfile.H264PROFILE_EXTENDED;
                    case CodecProfileLevel.AVCProfileHigh:
                    case CodecProfileLevel.AVCProfileConstrainedHigh:
                        return VideoCodecProfile.H264PROFILE_HIGH;
                    case CodecProfileLevel.AVCProfileHigh10:
                        return VideoCodecProfile.H264PROFILE_HIGH10PROFILE;
                    case CodecProfileLevel.AVCProfileHigh422:
                        return VideoCodecProfile.H264PROFILE_HIGH422PROFILE;
                    case CodecProfileLevel.AVCProfileHigh444:
                        return VideoCodecProfile.H264PROFILE_HIGH444PREDICTIVEPROFILE;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.VP8:
                switch (profile) {
                    case CodecProfileLevel.VP8ProfileMain:
                        return VideoCodecProfile.VP8PROFILE_ANY;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.VP9:
                switch (profile) {
                    case CodecProfileLevel.VP9Profile0:
                        return VideoCodecProfile.VP9PROFILE_PROFILE0;
                    case CodecProfileLevel.VP9Profile1:
                        return VideoCodecProfile.VP9PROFILE_PROFILE1;
                    case CodecProfileLevel.VP9Profile2:
                    case CodecProfileLevel.VP9Profile2HDR:
                    case CodecProfileLevel.VP9Profile2HDR10Plus:
                        return VideoCodecProfile.VP9PROFILE_PROFILE2;
                    case CodecProfileLevel.VP9Profile3:
                    case CodecProfileLevel.VP9Profile3HDR:
                        return VideoCodecProfile.VP9PROFILE_PROFILE3;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.AV1:
                switch (profile) {
                    case CodecProfileLevel.AV1ProfileMain8:
                    case CodecProfileLevel.AV1ProfileMain10:
                    case CodecProfileLevel.AV1ProfileMain10HDR10:
                    case CodecProfileLevel.AV1ProfileMain10HDR10Plus:
                        return VideoCodecProfile.AV1PROFILE_PROFILE_MAIN;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.HEVC:
                switch (profile) {
                    case CodecProfileLevel.HEVCProfileMain:
                        return VideoCodecProfile.HEVCPROFILE_MAIN;
                    case CodecProfileLevel.HEVCProfileMainStill:
                        return VideoCodecProfile.HEVCPROFILE_MAIN_STILL_PICTURE;
                    case CodecProfileLevel.HEVCProfileMain10:
                    case CodecProfileLevel.HEVCProfileMain10HDR10:
                    case CodecProfileLevel.HEVCProfileMain10HDR10Plus:
                        return VideoCodecProfile.HEVCPROFILE_MAIN10;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.DOLBY_VISION:
                switch (profile) {
                        // Profile 0, 1, 2, 3, 4, 6 are not supported for new applications.
                    case CodecProfileLevel.DolbyVisionProfileDvheStn:
                        return VideoCodecProfile.DOLBYVISION_PROFILE5;
                    case CodecProfileLevel.DolbyVisionProfileDvheDtb:
                        return VideoCodecProfile.DOLBYVISION_PROFILE7;
                    case CodecProfileLevel.DolbyVisionProfileDvheSt:
                        return VideoCodecProfile.DOLBYVISION_PROFILE8;
                    case CodecProfileLevel.DolbyVisionProfileDvavSe:
                        return VideoCodecProfile.DOLBYVISION_PROFILE9;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            default:
                throw new UnsupportedCodecProfileException();
        }
    }

    public static int mediaCodecLevelToChromiumMediaLevel(int codec, int level) {
        switch (codec) {
            case VideoCodec.H264:
                switch (level) {
                    case CodecProfileLevel.AVCLevel1b:
                        return 9;
                    case CodecProfileLevel.AVCLevel1:
                        return 10;
                    case CodecProfileLevel.AVCLevel11:
                        return 11;
                    case CodecProfileLevel.AVCLevel12:
                        return 12;
                    case CodecProfileLevel.AVCLevel13:
                        return 13;
                    case CodecProfileLevel.AVCLevel2:
                        return 20;
                    case CodecProfileLevel.AVCLevel21:
                        return 21;
                    case CodecProfileLevel.AVCLevel22:
                        return 22;
                    case CodecProfileLevel.AVCLevel3:
                        return 30;
                    case CodecProfileLevel.AVCLevel31:
                        return 31;
                    case CodecProfileLevel.AVCLevel32:
                        return 32;
                    case CodecProfileLevel.AVCLevel4:
                        return 40;
                    case CodecProfileLevel.AVCLevel41:
                        return 41;
                    case CodecProfileLevel.AVCLevel42:
                        return 42;
                    case CodecProfileLevel.AVCLevel5:
                        return 50;
                    case CodecProfileLevel.AVCLevel51:
                        return 51;
                    case CodecProfileLevel.AVCLevel52:
                        return 52;
                    case CodecProfileLevel.AVCLevel61:
                        return 61;
                    case CodecProfileLevel.AVCLevel62:
                        return 62;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.VP8:
                switch (level) {
                    case CodecProfileLevel.VP8Level_Version0:
                        return 0;
                    case CodecProfileLevel.VP8Level_Version1:
                        return 1;
                    case CodecProfileLevel.VP8Level_Version2:
                        return 2;
                    case CodecProfileLevel.VP8Level_Version3:
                        return 3;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.VP9:
                switch (level) {
                    case CodecProfileLevel.VP9Level1:
                        return 10;
                    case CodecProfileLevel.VP9Level11:
                        return 11;
                    case CodecProfileLevel.VP9Level2:
                        return 20;
                    case CodecProfileLevel.VP9Level21:
                        return 21;
                    case CodecProfileLevel.VP9Level3:
                        return 30;
                    case CodecProfileLevel.VP9Level31:
                        return 31;
                    case CodecProfileLevel.VP9Level4:
                        return 40;
                    case CodecProfileLevel.VP9Level41:
                        return 41;
                    case CodecProfileLevel.VP9Level5:
                        return 50;
                    case CodecProfileLevel.VP9Level51:
                        return 51;
                    case CodecProfileLevel.VP9Level52:
                        return 52;
                    case CodecProfileLevel.VP9Level6:
                        return 60;
                    case CodecProfileLevel.VP9Level61:
                        return 61;
                    case CodecProfileLevel.VP9Level62:
                        return 62;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.AV1:
                switch (level) {
                    case CodecProfileLevel.AV1Level2:
                        return 20;
                    case CodecProfileLevel.AV1Level21:
                        return 21;
                    case CodecProfileLevel.AV1Level22:
                        return 22;
                    case CodecProfileLevel.AV1Level23:
                        return 23;
                    case CodecProfileLevel.AV1Level3:
                        return 30;
                    case CodecProfileLevel.AV1Level31:
                        return 31;
                    case CodecProfileLevel.AV1Level32:
                        return 32;
                    case CodecProfileLevel.AV1Level33:
                        return 33;
                    case CodecProfileLevel.AV1Level4:
                        return 40;
                    case CodecProfileLevel.AV1Level41:
                        return 41;
                    case CodecProfileLevel.AV1Level42:
                        return 42;
                    case CodecProfileLevel.AV1Level43:
                        return 43;
                    case CodecProfileLevel.AV1Level5:
                        return 50;
                    case CodecProfileLevel.AV1Level51:
                        return 51;
                    case CodecProfileLevel.AV1Level52:
                        return 52;
                    case CodecProfileLevel.AV1Level53:
                        return 53;
                    case CodecProfileLevel.AV1Level6:
                        return 60;
                    case CodecProfileLevel.AV1Level61:
                        return 61;
                    case CodecProfileLevel.AV1Level62:
                        return 62;
                    case CodecProfileLevel.AV1Level63:
                        return 63;
                    case CodecProfileLevel.AV1Level7:
                        return 70;
                    case CodecProfileLevel.AV1Level71:
                        return 71;
                    case CodecProfileLevel.AV1Level72:
                        return 72;
                    case CodecProfileLevel.AV1Level73:
                        return 73;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.HEVC:
                switch (level) {
                    case CodecProfileLevel.HEVCHighTierLevel1:
                    case CodecProfileLevel.HEVCMainTierLevel1:
                        return 30;
                    case CodecProfileLevel.HEVCHighTierLevel2:
                    case CodecProfileLevel.HEVCMainTierLevel2:
                        return 60;
                    case CodecProfileLevel.HEVCHighTierLevel21:
                    case CodecProfileLevel.HEVCMainTierLevel21:
                        return 63;
                    case CodecProfileLevel.HEVCHighTierLevel3:
                    case CodecProfileLevel.HEVCMainTierLevel3:
                        return 90;
                    case CodecProfileLevel.HEVCHighTierLevel31:
                    case CodecProfileLevel.HEVCMainTierLevel31:
                        return 93;
                    case CodecProfileLevel.HEVCHighTierLevel4:
                    case CodecProfileLevel.HEVCMainTierLevel4:
                        return 120;
                    case CodecProfileLevel.HEVCHighTierLevel41:
                    case CodecProfileLevel.HEVCMainTierLevel41:
                        return 123;
                    case CodecProfileLevel.HEVCHighTierLevel5:
                    case CodecProfileLevel.HEVCMainTierLevel5:
                        return 150;
                    case CodecProfileLevel.HEVCHighTierLevel51:
                    case CodecProfileLevel.HEVCMainTierLevel51:
                        return 153;
                    case CodecProfileLevel.HEVCHighTierLevel52:
                    case CodecProfileLevel.HEVCMainTierLevel52:
                        return 156;
                    case CodecProfileLevel.HEVCHighTierLevel6:
                    case CodecProfileLevel.HEVCMainTierLevel6:
                        return 180;
                    case CodecProfileLevel.HEVCHighTierLevel61:
                    case CodecProfileLevel.HEVCMainTierLevel61:
                        return 183;
                    case CodecProfileLevel.HEVCHighTierLevel62:
                    case CodecProfileLevel.HEVCMainTierLevel62:
                        return 186;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            case VideoCodec.DOLBY_VISION:
                switch (level) {
                    case CodecProfileLevel.DolbyVisionLevelHd24:
                        return 1;
                    case CodecProfileLevel.DolbyVisionLevelHd30:
                        return 2;
                    case CodecProfileLevel.DolbyVisionLevelFhd24:
                        return 3;
                    case CodecProfileLevel.DolbyVisionLevelFhd30:
                        return 4;
                    case CodecProfileLevel.DolbyVisionLevelFhd60:
                        return 5;
                    case CodecProfileLevel.DolbyVisionLevelUhd24:
                        return 6;
                    case CodecProfileLevel.DolbyVisionLevelUhd30:
                        return 7;
                    case CodecProfileLevel.DolbyVisionLevelUhd48:
                        return 8;
                    case CodecProfileLevel.DolbyVisionLevelUhd60:
                        return 9;
                    default:
                        throw new UnsupportedCodecProfileException();
                }
            default:
                throw new UnsupportedCodecProfileException();
        }
    }
}
