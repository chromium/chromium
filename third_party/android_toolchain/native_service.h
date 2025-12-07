// This file is temporary until Android actually ships an NDK with this header.
// This is a copy from http://ag/34101390, with __attribute__((weak)) added to
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
 * {@link ANativeService} represents a native service instance.
 * A unique instance of this struct is prepared by the framework for each service and it lives
 * during the service's lifetime. The same instance is passed to all callback functions of the
 * service.
 *
 * Introduced in API 37.
 */
typedef struct ANativeService ANativeService;

/**
 * The function type signature definition of the entry point function of the service.
 * `service` must be initialized in this function.
 *
 * This function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 */
typedef void ANativeService_createFunc(ANativeService* _Nonnull service);

/**
 * The default name of the entry point function. You can specify a different function name through
 * `android.app.func_name` meta-data in your manifest.
 *
 * Introduced in API 37.
 */
extern ANativeService_createFunc ANativeService_onCreate;

/**
 * The levels for {@link ANativeService_onTrimMemoryCallback} indicating the context of the trim,
 * giving a hint of the amount of trimming the application may like to perform.
 *
 * Introduced in API 37.
 */
typedef enum ANativeServiceTrimMemoryLevel : int32_t {
    /**
     * The process had been showing a user interface, and is no longer doing so.  Large allocations
     * with the UI should be released at this point to allow memory to be better managed.
     *
     * Introduced in API 37.
     */
    ANATIVE_SERVICE_TRIM_MEMORY_UI_HIDDEN = 20,

    /**
     * The process has gone on to the LRU list.  This is a good opportunity to clean up resources
     * that can efficiently and quickly be re-built if the user returns to the app.
     *
     * Introduced in API 37.
     */
    ANATIVE_SERVICE_TRIM_MEMORY_BACKGROUND = 40,
} ANativeServiceTrimMemoryLevel;

/**
 * The function type signature definition of the "onBind" callback function called when someone is
 * binding to the service, with the given action and data on the intent. This may return NULL if
 * clients cannot bind to the service, or a pointer to a valid AIBinder. If an AIBinder is returned
 * you *must* first call AIBinder_incStrong() on the binder returning it with a single strong
 * reference. If you do not you will see crashes about referencing a pure virtual function, as the
 * instance will be destructed when returning from your onBind() implementation.
 *
 * This callback function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param intentToken A token associated with the intent that was used to bind to this
 * service, as given to `Context.bindService`.
 * \param action The action specified in the intent passed to `Context.bindService` or null if not
 * specified.
 * \param data The data specified in the intent passed to `Context.bindService`. This is an encoded
 * URI or null if not specified.
 * \return an AIBinder pointer through which clients can call on to the service.
 */
typedef AIBinder* _Nullable (*ANativeService_onBindCallback)(ANativeService* _Nonnull service,
                                                             int32_t intentToken,
                                                             char const* _Nullable action,
                                                             char const* _Nullable data);

/**
 * The function type signature definition of the "onUnbind" callback function called when all
 * clients have disconnected from a particular interface published by the service. Return true if
 * you would like to have the service's onRebind() method later called when new clients bind to it.
 *
 * This callback function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param intentToken A token associated with the intent that was used to bind to this
 * service, as given to `Context.bindService`.
 * \return true if you would like to have the service's {@link ANativeService_onRebindCallback}
 * callback later called when new clients bind to it, otherwise false.
 */
typedef bool (*ANativeService_onUnbindCallback)(ANativeService* _Nonnull service,
                                                int32_t intentToken);

/**
 * The function type signature definition of the "onRebind" callback function called when someone is
 * rebinding to the service. This callback is called only when onUnbind() returned true before.
 *
 * This callback function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param intentToken A token associated with the intent that was used to bind to this
 * service, as given to `Context.bindService`.
 * service binding.
 */
typedef void (*ANativeService_onRebindCallback)(ANativeService* _Nonnull service,
                                                int32_t intentToken);

/**
 * The function type signature definition of the "onDestroy" callback function called when the
 * service is being destroyed.
 *
 * This callback function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 */
typedef void (*ANativeService_onDestroyCallback)(ANativeService* _Nonnull service);

/**
 * The function type signature definition of the "onTrimMemory" callback function called when the
 * operating system has determined that it is a good time for a process to trim unneeded memory from
 * its process.
 *
 * You should never compare to exact values of the level, since new intermediate values may be added
 * -- you will typically want to compare if the value is greater or equal to a level you are
 * interested in.
 *
 * This callback function will run on the main thread of the process.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param level {@link ANativeServiceTrimMemoryLevel} indicating the context of the trim, giving a
 * hint of the amount of trimming the application may like to perform.
 */
typedef void (*ANativeService_onTrimMemoryCallback)(ANativeService* _Nonnull service,
                                                    ANativeServiceTrimMemoryLevel level);

/**
 * Sets the "onBind" callback function for the service.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param callback A pointer to an implementation of {@link ANativeService_onBindCallback}.
 */
void ANativeService_setOnBindCallback(ANativeService* _Nonnull service,
                                      ANativeService_onBindCallback _Nullable callback)
        __attribute__((weak));

/**
 * Sets the "onUnbind" callback function for the service.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param callback A pointer to an implementation of {@link ANativeService_onUnbindCallback}.
 */
void ANativeService_setOnUnbindCallback(ANativeService* _Nonnull service,
                                        ANativeService_onUnbindCallback _Nullable callback)
        __attribute__((weak));

/**
 * Sets the "onRebind" callback function for the service.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param callback A pointer to an implementation {@link ANativeService_onRebindCallback}.
 */
void ANativeService_setOnRebindCallback(ANativeService* _Nonnull service,
                                        ANativeService_onRebindCallback _Nullable callback)
        __attribute__((weak));

/**
 * Sets the "onDestroy" callback function for the service.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param callback A pointer to an implementation of {@link ANativeService_onDestroyCallback}.
 */
void ANativeService_setOnDestroyCallback(ANativeService* _Nonnull service,
                                         ANativeService_onDestroyCallback _Nullable callback)
        __attribute__((weak));

/**
 * Sets the "onTrimMemory" callback function for the service.
 *
 * Introduced in API 37.
 *
 * \param service {@link ANativeService} associated with the service.
 * \param callback A pointer to an implementation of {@link ANativeService_onTrimMemoryCallback}.
 */
void ANativeService_setOnTrimMemoryCallback(ANativeService* _Nonnull service,
                                            ANativeService_onTrimMemoryCallback _Nullable callback)
        __attribute__((weak));

__END_DECLS

#endif // ANDROID_NATIVE_SERVICE_H

/** @} */
