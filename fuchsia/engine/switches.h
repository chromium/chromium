// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_CONTEXT_SWITCHES_H_
#define FUCHSIA_ENGINE_CONTEXT_SWITCHES_H_

// Switches used by the ContextProviderImpl to configure each Context process.
namespace switches {

// Register custom content directories under the fuchsia-dir:// scheme. Value
// is a comma-separated list of key=value pairs mapping a directory name to a
// fuchsia.io.Directory handle Id, e.g. foo=1234,bar=5678
extern const char kContentDirectories[];

// Prevents the use of video codecs that are not hardware-accelerated.
extern const char kDisableSoftwareVideoDecoders[];

// Enables Widevine CDM support.
extern const char kEnableWidevine[];

// Indicates that the Context was created without a |data_directory|.
extern const char kIncognito[];

// Enables PlayReady CDM and specifies the corresponding key system string.
extern const char kPlayreadyKeySystem[];

// Enables debug-mode DevTools. Value consists of a comma-separated list of
// handle-Ids to retrieve via |zx_take_startup_handle()|.
extern const char kRemoteDebuggerHandles[];

// Specifies a custom UserAgent product & version to use.
extern const char kUserAgentProductAndVersion[];

// By default, an https page cannot run JavaScript, CSS or resources from http
// URLs. This provides an override to get the old insecure behavior.
// TODO(crbug.com/1023514): Remove this switch when it is no longer
// necessary.
extern const char kAllowRunningInsecureContent[];

}  // namespace switches

#endif  // FUCHSIA_ENGINE_SWITCHES_H_
