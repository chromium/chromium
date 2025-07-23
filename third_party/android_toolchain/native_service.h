// This file is temporary until Android actually ships an NDK with this header.
// This is a copy from http://ag/34101390/3, with __attribute__((weak)) added to
// appropriate functions, as this isn't officially a part of the NDK and thus
// __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ (which we always use) isn't applying
// to this yet.
/*
 * Copyright (C) 2025 The Android Open Source Project
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

/**
 * @addtogroup NativeService Native Service
 * @{
 */

/**
 * @file native_service.h
 */

#ifndef ANDROID_NATIVE_SERVICE_H
#define ANDROID_NATIVE_SERVICE_H

#include <sys/cdefs.h>

#include <android/binder_ibinder.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * An opaque struct that represents a native service instance.
 * An instance of this struct is prepared by the framework and lives for the native service's
 * lifetime. The same instance is passed to all callback functions of the service.
 */
struct ANativeService;
typedef struct ANativeService ANativeService;

/*
 * Function prototype definition of the entry point function of native services.
 * The service instance must be initialized in this function.
 */
typedef void ANativeService_createFunc(ANativeService* _Nonnull service);

/**
 * The default name of the entry point function. You can specify a different function name through
 * "android.app.func_name" meta-data in your manifest.
 */
extern ANativeService_createFunc ANativeService_onCreate;

/**
 * Function prototype definitions of native service callback functions.
 */

/**
 * Someone is binding to the service, with the given action on the intent. This may return
 * NULL, or a pointer to a valid AIBinder. If an AIBinder is returned you *must* first
 * call AIBinder_incStrong() on the binder returning it with a single strong reference.
 * If you do not you will see crashes about referencing a pure virtual function, as the instance
 * will be destructed when returning from your onBind() implementation.
 */
typedef AIBinder* _Nullable (*ANativeService_onBindCallback)(ANativeService* _Nonnull service,
                                                             void const* _Nullable bindToken,
                                                             char const* _Nullable action);
/**
 * TODO: docs
 */
typedef bool (*ANativeService_onUnbindCallback)(ANativeService* _Nonnull service,
                                                void const* _Nullable bindToken);
/**
 * TODO: docs
 */
typedef void (*ANativeService_onRebindCallback)(ANativeService* _Nonnull service,
                                                void const* _Nullable bindToken);
/**
 * The native service is being destroyed. See Java documentation for Service.onDestroy()
 * for more information.
 */
typedef void (*ANativeService_onDestroyCallback)(ANativeService* _Nonnull service);
/**
 * The system is running low on memory. Use this callback to release
 * resources you do not need, to help the system avoid killing more
 * important processes.
 */
typedef void (*ANativeService_onLowMemoryCallback)(ANativeService* _Nonnull service);

/*
 * Setter functions to manipulate ANativeService.
 */
void ANativeService_setOnBindCallback(ANativeService* _Nonnull service,
                                      ANativeService_onBindCallback _Nullable callback) __attribute__((weak));
void ANativeService_setOnUnbindCallback(ANativeService* _Nonnull service,
                                        ANativeService_onUnbindCallback _Nullable callback) __attribute__((weak));
void ANativeService_setOnRebindCallback(ANativeService* _Nonnull service,
                                        ANativeService_onRebindCallback _Nullable callback) __attribute__((weak));
void ANativeService_setOnDestroyCallback(ANativeService* _Nonnull service,
                                         ANativeService_onDestroyCallback _Nullable callback) __attribute__((weak));
void ANativeService_setOnLowMemoryCallback(ANativeService* _Nonnull service,
                                           ANativeService_onLowMemoryCallback _Nullable callback) __attribute__((weak));
void ANativeService_setApplicationContext(ANativeService* _Nonnull service,
                                          void* _Nullable context) __attribute__((weak));

/*
 * Getter functions to retrieve information from ANativeService.
 */
void* _Nullable ANativeService_getApplicationContext(const ANativeService* _Nonnull service);

__END_DECLS

#endif // ANDROID_NATIVE_SERVICE_H

/** @} */
