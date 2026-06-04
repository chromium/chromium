/*
 * Copyright 2022 The Android Open Source Project
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
#include <jni.h>
#include <string>
#include <poll.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/log.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/sync.h>
#include <android/hardware_buffer_jni.h>
#include <mutex>
#include "egl_utils.h"

#define EGL_UTILS "EglUtils"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, EGL_UTILS, __VA_ARGS__)

/**
 * Cached reference to the eglGetNativeClientBufferANDROID egl extension method
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID = nullptr;

/**
 * Cached reference to the eglImageTargetTexture2DOES egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

/**
 * Cached reference to the eglCreateImageKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;

/**
 * Cached reference to the eglDestroyImageKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;

/**
 * Cached reference to the eglCreateSyncKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR = nullptr;

/**
 * Cached reference to the eglGetSyncAttribKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR = nullptr;

/**
 * Cached reference to the eglClientWaitSyncKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR = nullptr;

/**
 * Cached reference to the eglDestroySyncKHR egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = nullptr;

/**
 * Cached reference to the eglDupNativeFenceFDANDROID egl extension method.
 * On first invocation within the corresponding JNI method, a call to eglGetProcAddress
 * is made to determine if this method exists. If it does then this function pointer
 * is persisted for subsequent method calls.
 */
static PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID = nullptr;

/**
 * Helper method for querying the EGL extension method eglGetNativeClientBufferANDROID.
 * This is used in initial invocations of the corresponding JNI method to obtain
 * an EGLClientBuffer instance from a HardwareBuffer as well as for testing purposes
 * to guarantee that Android devices that advertise support for the corresponding
 * extensions actually expose this API.
 */
static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC obtainEglGetNativeClientBufferANDROID() {
    return reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
            eglGetProcAddress("eglGetNativeClientBufferANDROID"));
}

/**
 * Helper method for querying the EGL extension method eglCreateImageKHR.
 * This is used in initial invocations of the corresponding JNI method to obtain
 * an EGLImage from an EGLClientBuffer as well as for testing purposes
 * to guarantee that Android devices that advertise support for the corresponding
 * extensions actually expose this API.
 */
static PFNEGLCREATEIMAGEKHRPROC obtainEglCreateImageKHR() {
    return reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
            eglGetProcAddress("eglCreateImageKHR"));
}

/**
 * Helper method for querying the EGL extension method eglDestroyImageKHR.
 * This is used in initial invocations of the corresponding JNI method to destroy
 * an EGLImage as well as for testing purposes to guarantee that Android devices
 * that advertise support for the corresponding extensions actually expose this API.
 */
static PFNEGLDESTROYIMAGEKHRPROC obtainEglDestroyImageKHR() {
    return reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
            eglGetProcAddress("eglDestroyImageKHR"));
}

/**
 * Helper method for querying the EGL extension method glImageTargetTexture2DOES.
 * This is used in initial invocations of the corresponding JNI method to load
 * an EGLImage instance into a caller defined GL Texture as well as for testing
 * purposes to guarantee that Android devices that advertise support for the
 * corresponding extensions actually expose this API.
 */
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC obtainGlImageTargetTexture2DOES() {
    return reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));
}

/**
 * Helper method for querying the EGL extension method eglDupNativeFenceFDANDROID.
 * This is used in initial invocations of the corresponding JNI method to to create EGL  fence sync
 * objects that are associated with a native synchronization fence object that are referenced using
 * a file descriptor. Additionally this is used for testing purposes to guarantee that Android
 * devices that advertise support for the corresponding extensions actually expose this API.
 */
static PFNEGLDUPNATIVEFENCEFDANDROIDPROC obtainEglDupNativeFenceFDANDROID() {
    return reinterpret_cast<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>(
            eglGetProcAddress("eglDupNativeFenceFDANDROID"));
}

/**
 * Helper method for querying the EGL extension method eglCreateSyncKHR.
 * This is used in initial invocations of the corresponding JNI method to to create EGL fence sync
 * object. Additionally this is used for testing purposes to guarantee that Android
 * devices that advertise support for the corresponding extensions actually expose this API.
 */
static PFNEGLCREATESYNCKHRPROC obtainEglCreateSyncKHR() {
    return reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(
            eglGetProcAddress("eglCreateSyncKHR"));
}

/**
 * Helper method for querying the EGL extension method eglGetSyncAttribKHR.
 * This is used in initial invocations of the corresponding JNI method to query
 * properties of an EGLSync object returned by eglCreateSyncKHR as well as for
 * testing purposes to guarantee that Android devices that advertise support for
 * the corresponding extensions actually expose this API.
 */
static PFNEGLGETSYNCATTRIBKHRPROC obtainEglGetSyncAttribKHR() {
    return reinterpret_cast<PFNEGLGETSYNCATTRIBKHRPROC>(
            eglGetProcAddress("eglGetSyncAttribKHR"));
}

/**
 * Helper method for querying the EGL extension method eglClientWaitSyncKHR.
 * This is used in initial invocations of the corresponding JNI method to block
 * the current thread until the specified sync object is signalled or until a timeout
 * has passed. Additionally this is used for testing purposes to guarantee that Android devices
 * that advertise support for the corresponding extensions actually expose this API.
 */
static PFNEGLCLIENTWAITSYNCKHRPROC obtainEglClientWaitSyncKHR() {
    return reinterpret_cast<PFNEGLCLIENTWAITSYNCKHRPROC>(
            eglGetProcAddress("eglClientWaitSyncKHR"));
}

/**
 * Helper method for querying the EGL extension method eglDestroySyncKHR.
 * This is used in initial invocations of the corresponding JNI method to destroy an EGL fence sync
 * object. Additionally this is used for testing purposes to guarantee that Android devices that
 * advertise support for corresponding extensions actually expose this API.
 */
static PFNEGLDESTROYSYNCKHRPROC obtainEglDestroySyncKHR() {
    return reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(
            eglGetProcAddress("eglDestroySyncKHR"));
}

jlong EGLBindings_nCreateImageFromHardwareBuffer(
        JNIEnv *env, jclass , jlong egl_display_ptr, jobject hardware_buffer) {
    static std::once_flag eglGetNativeClientBufferANDROIDFlag;
    static std::once_flag eglCreateImageKHRFlag;
    std::call_once(eglGetNativeClientBufferANDROIDFlag, [](){
        eglGetNativeClientBufferANDROID = obtainEglGetNativeClientBufferANDROID();
    });
    if (!eglGetNativeClientBufferANDROID) {
        ALOGE("Unable to resolve eglGetNativeClientBufferANDROID");
        return 0;
    }

    std::call_once(eglCreateImageKHRFlag, [](){
        eglCreateImageKHR = obtainEglCreateImageKHR();
    });
    if (!eglCreateImageKHR) {
        ALOGE("Unable to resolve eglCreateImageKHR");
        return 0;
    }

    AHardwareBuffer *buffer =
            AHardwareBuffer_fromHardwareBuffer(env, hardware_buffer);
    EGLClientBuffer eglClientBuffer = eglGetNativeClientBufferANDROID(buffer);

    EGLint imageAttrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    EGLImage image = eglCreateImageKHR(
            display,
            EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID,
            eglClientBuffer,
            imageAttrs
    );

    return reinterpret_cast<jlong>(image);
}

jboolean EGLBindings_nDestroyImageKHR(
        JNIEnv *env, jclass, jlong egl_display_ptr, jlong egl_image_ptr) {
    static std::once_flag eglDestroyImageKHRFlag;
    std::call_once(eglDestroyImageKHRFlag, [](){
        eglDestroyImageKHR = obtainEglDestroyImageKHR();
    });
    if (!eglDestroyImageKHR) {
        ALOGE("Unable to resolve eglDestroyImageKHR");
        return static_cast<jboolean>(false);
    }

    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto eglImage = reinterpret_cast<EGLImage>(egl_image_ptr);
    return static_cast<jboolean>(eglDestroyImageKHR(display, eglImage));
}

void EGLBindings_nImageTargetTexture2DOES(JNIEnv *env, jclass, jint target, jlong egl_image_ptr) {
    static std::once_flag glEGLImageTargetTexture2DOESFlag;
    std::call_once(glEGLImageTargetTexture2DOESFlag, [](){
        glEGLImageTargetTexture2DOES = obtainGlImageTargetTexture2DOES();
    });
    if (!glEGLImageTargetTexture2DOES) {
        ALOGE("Unable to resolve glEGLImageTargetTexture2DOES");
        return;
    }

    glEGLImageTargetTexture2DOES(target, reinterpret_cast<EGLImage>(egl_image_ptr));
}

jint EGLBindings_nDupNativeFenceFDANDROID(JNIEnv *env, jclass, jlong egl_display_ptr,
                                          jlong sync_ptr) {
    static std::once_flag eglDupNativeFenceFDANDROIDflag;
    std::call_once(eglDupNativeFenceFDANDROIDflag, [](){
        eglDupNativeFenceFDANDROID = obtainEglDupNativeFenceFDANDROID();
    });
    if (!eglDupNativeFenceFDANDROID) {
        ALOGE("Unable to resolve eglDupNativeFenceFDAndroid");
        return EGL_NO_NATIVE_FENCE_FD_ANDROID;
    }

    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto sync = reinterpret_cast<EGLSync>(sync_ptr);
    return (jint)eglDupNativeFenceFDANDROID(display, sync);
}

jlong EGLBindings_nCreateSyncKHR(JNIEnv *env, jclass, jlong egl_display_ptr, jint type,
                                 jintArray attrs) {
    static std::once_flag eglCreateSyncKHRFlag;
    std::call_once(eglCreateSyncKHRFlag, [](){
        eglCreateSyncKHR = obtainEglCreateSyncKHR();
    });
    if (!eglCreateSyncKHR) {
        ALOGE("Unable to resolve eglCreateSyncKHR");
        return 0;
    }

    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto attrib_list = reinterpret_cast<EGLint *>(attrs);
    return reinterpret_cast<jlong>(eglCreateSyncKHR(display, type, attrib_list));
}

static jobject createIllegalArgumentException(JNIEnv* env, jstring message) {
    jclass exceptionClass = env->FindClass("java/lang/IllegalArgumentException");
    if (exceptionClass != nullptr) {
        jmethodID init = env->GetMethodID(exceptionClass, "<init>", "(Ljava/lang/String;)V");
        jobject instance;
        if (init != nullptr) {
            instance = env->NewObject(exceptionClass, init, message);
        } else {
            ALOGE("Unable to find constructor for IllegalArgumentException");
            instance = nullptr;
        }
        env->DeleteLocalRef(exceptionClass);
        return instance;
    } else {
        ALOGE("Unable to find IllegalArgumentException class");
        return nullptr;
    }
}

static int jniThrowIllegalArgumentException(JNIEnv* env, const char* msg) {
    jstring message = env->NewStringUTF(msg);
    if (message != nullptr) {
        jobject exception = createIllegalArgumentException(env, message);
        int status = 0;
        if (exception != nullptr) {
            if (env->Throw((jthrowable) exception) != JNI_OK) {
                ALOGE("Unable to throw IllegalArgumentException");
                status = -1;
            }
        } else {
            status = -1;
        }
        env->DeleteLocalRef(message);
        return status;
    } else {
        env->ExceptionClear();
        return -1;
    }
}

jboolean EGLBindings_nGetSyncAttribKHR(
        JNIEnv *env,
        jclass,
        jlong egl_display_ptr,
        jlong sync_ptr,
        jint attrib,
        jintArray result_ref,
        jint offset
        ) {
    static std::once_flag eglGetSyncAttribKHRFlag;
    std::call_once(eglGetSyncAttribKHRFlag, []() {
        eglGetSyncAttribKHR = obtainEglGetSyncAttribKHR();
    });
    if (!eglGetSyncAttribKHR) {
        ALOGE("Unable to resolve eglGetSyncAttribKHR");
        return static_cast<jboolean>(false);
    }

    if (!result_ref) {
        jniThrowIllegalArgumentException(env,
            "Null pointer received, invalid array provided to store eglGetSyncAttribKHR result");
        return static_cast<jboolean>(false);
    }

    if (offset < 0) {
        jniThrowIllegalArgumentException(env,
            "Invalid offset provided, must be greater than or equal to 0");
        return static_cast<jboolean>(false);
    }

    jint remaining = env->GetArrayLength(result_ref) - offset;
    if (remaining < 1) {
        jniThrowIllegalArgumentException(env, "length - offset is out of bounds");
        return static_cast<jboolean>(false);
    }

    auto result_base = (GLint *)env->GetIntArrayElements(result_ref, (jboolean *)nullptr);
    auto result = (GLint *)(result_base + offset);
    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto sync = reinterpret_cast<EGLSync>(sync_ptr);
    auto success = static_cast<jboolean>(eglGetSyncAttribKHR(display, sync, attrib, result));
    env->ReleaseIntArrayElements(result_ref, (jint*) result_base, 0);
    return success;
}

jint EGLBindings_nClientWaitSyncKHR(
        JNIEnv *env,
        jclass,
        jlong egl_display_ptr,
        jlong sync_ptr,
        jint flags,
        jlong timeout
        ) {
    static std::once_flag eglClientWaitKRFlag;
    std::call_once(eglClientWaitKRFlag, []() {
        eglClientWaitSyncKHR = obtainEglClientWaitSyncKHR();
    });

    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto sync = reinterpret_cast<EGLSync>(sync_ptr);
    auto wait_flags = static_cast<EGLint>(flags);
    auto wait_timeout = static_cast<EGLTimeKHR>(timeout);
    return static_cast<jint>(eglClientWaitSyncKHR(display, sync, wait_flags, wait_timeout));
}

jboolean EGLBindings_nDestroySyncKHR(
        JNIEnv *env,
        jclass,
        jlong egl_display_ptr,
        jlong sync_ptr
        ) {
    static std::once_flag eglDestroySyncKHRFlag;
    std::call_once(eglDestroySyncKHRFlag, [](){
        eglDestroySyncKHR = obtainEglDestroySyncKHR();
    });
    if (!eglDestroySyncKHR) {
        ALOGE("Unable to resolve eglDestroySyncKHR");
        return static_cast<jboolean>(false);
    }

    auto display = reinterpret_cast<EGLDisplay *>(egl_display_ptr);
    auto sync = reinterpret_cast<EGLSync>(sync_ptr);
    return static_cast<jboolean>(eglDestroySyncKHR(display, sync));
}

/**
 * Helper method used in testing to verify if the eglGetNativeClientBufferANDROID method
 * is actually supported on the Android device.
 */
jboolean EGLBindings_nSupportsEglGetNativeClientBufferAndroid(
        JNIEnv *env, jclass) {
    return obtainEglGetNativeClientBufferANDROID() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglCreateImageKHR method
 * is actually supported on the Android device.
 */
jboolean EGLBindings_nSupportsEglCreateImageKHR(JNIEnv *env, jclass) {
    return obtainEglCreateImageKHR() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglDestroyImageKHR method
 * is actually supported on the Android device.
 */
jboolean EGLBindings_nSupportsEglDestroyImageKHR(JNIEnv *env, jclass) {
    return obtainEglDestroyImageKHR() != nullptr;
}

/**
 * Helper method used in testing to verify if the glImageTargetTexture2DOES method
 * is actually supported on the Android device.
 */
jboolean EGLBindings_nSupportsGlImageTargetTexture2DOES(JNIEnv *env, jclass) {
    return obtainGlImageTargetTexture2DOES() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglCreateSyncKHR method is actually supported
 * on the Android device
 */
jboolean EGLBindings_nSupportsEglCreateSyncKHR(JNIEnv *env, jclass) {
    return obtainEglCreateSyncKHR() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglDestroySyncKHR method is actually supported
 * on the Android device
 */
jboolean EGLBindings_nSupportsEglDestroySyncKHR(JNIEnv *env, jclass) {
    return obtainEglDestroySyncKHR() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglDupNativeFenceFDAndroid methid is actually
 * supported on the Android device
 */
jboolean EGLBindings_nSupportsDupNativeFenceFDANDROID(JNIEnv *env, jclass) {
    return obtainEglDupNativeFenceFDANDROID() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglGetSyncAttribKHR method is actually supported
 * on the Android device
 */
jboolean EGLBindings_nSupportsEglGetSyncAttribKHR(JNIEnv *env, jclass) {
    return obtainEglGetSyncAttribKHR() != nullptr;
}

/**
 * Helper method used in testing to verify if the eglClientWaitSyncKHR method is actually supported
 * on the Android device
 */
jboolean EGLBindings_nSupportsEglClientWaitSyncKHR(JNIEnv *env, jclass) {
    return obtainEglClientWaitSyncKHR() != nullptr;
}

/**
 * Java does not support unsigned long types. Ensure that our casting of Java types the native
 * equivalent matches.
 */
jboolean EGLBindings_nEqualToNativeForeverTimeout(JNIEnv *env, jclass, jlong timeout_nanos) {
    return static_cast<EGLTimeKHR>(timeout_nanos) == EGL_FOREVER_KHR;
}

static const JNINativeMethod EGL_METHOD_TABLE[] = {
        {
            "nCreateImageFromHardwareBuffer",
            "(JLandroid/hardware/HardwareBuffer;)J",
            (void*)EGLBindings_nCreateImageFromHardwareBuffer
        },
        {
            "nDestroyImageKHR",
            "(JJ)Z",
            (void*)EGLBindings_nDestroyImageKHR
        },
        {
            "nImageTargetTexture2DOES",
            "(IJ)V",
            (void*)EGLBindings_nImageTargetTexture2DOES
        },
        {
            "nDupNativeFenceFDANDROID",
            "(JJ)I",
            (void*)EGLBindings_nDupNativeFenceFDANDROID
        },
        {
            "nCreateSyncKHR",
            "(JI[I)J",
            (void*)EGLBindings_nCreateSyncKHR
        },
        {
            "nGetSyncAttribKHR",
            "(JJI[II)Z",
            (void*)EGLBindings_nGetSyncAttribKHR
        },
        {
            "nClientWaitSyncKHR",
            "(JJIJ)I",
            (void*)EGLBindings_nClientWaitSyncKHR
        },
        {
            "nDestroySyncKHR",
            "(JJ)Z",
            (void*)EGLBindings_nDestroySyncKHR
        },
        {
            "nSupportsEglGetNativeClientBufferAndroid",
            "()Z",
            (void*)EGLBindings_nSupportsEglGetNativeClientBufferAndroid
        },
        {
            "nSupportsEglCreateImageKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglCreateImageKHR
        },
        {
            "nSupportsEglDestroyImageKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglDestroyImageKHR
        },
        {
            "nSupportsGlImageTargetTexture2DOES",
            "()Z",
            (void*)EGLBindings_nSupportsGlImageTargetTexture2DOES
        },
        {
            "nSupportsEglCreateSyncKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglCreateSyncKHR
        },
        {
            "nSupportsEglDestroySyncKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglDestroySyncKHR
        },
        {
            "nSupportsDupNativeFenceFDANDROID",
            "()Z",
            (void*)EGLBindings_nSupportsDupNativeFenceFDANDROID
        },
        {
            "nSupportsEglGetSyncAttribKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglGetSyncAttribKHR
        },
        {
            "nSupportsEglClientWaitSyncKHR",
            "()Z",
            (void*)EGLBindings_nSupportsEglClientWaitSyncKHR
        },
        {
            "nEqualToNativeForeverTimeout",
            "(J)Z",
            (void*)EGLBindings_nEqualToNativeForeverTimeout
        }
};

jint loadEGLMethods(JNIEnv* env) {
    jclass eglBindingsClass = env->FindClass("androidx/opengl/EGLBindings");
    if (eglBindingsClass == nullptr) {
        return JNI_ERR;
    }
    if (env->RegisterNatives(eglBindingsClass, EGL_METHOD_TABLE,
                             sizeof(EGL_METHOD_TABLE) / sizeof(JNINativeMethod)) != JNI_OK) {
        return JNI_ERR;
    }
    return JNI_OK;
}