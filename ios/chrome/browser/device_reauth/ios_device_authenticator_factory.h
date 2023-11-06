// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_

#import <memory>

class ChromeBrowserState;
class IOSDeviceAuthenticator;

namespace device_reauth {
class DeviceAuthParams;
}

@protocol ReauthenticationProtocol;

// Creates an IOSDeviceAuthenticator. It is built on top of a
// DeviceAuthenticatorProxy. `reauth_module` is the component that provides the
// device reauth functionalities. `browser_state` is the ChromeBrowserState the
// underlying DeviceAuthenticatorProxy is attached to. `params` contains configs
// for the authentication.
std::unique_ptr<IOSDeviceAuthenticator> CreateIOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    ChromeBrowserState* browser_state,
    const device_reauth::DeviceAuthParams& params);

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_
