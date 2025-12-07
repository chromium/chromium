/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_H_

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/platform/forward_declared_member.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class AuthenticationCredentialsContainer;
class BatteryManager;
class Bluetooth;
class Clipboard;
class ContactsManager;
class ContextLifecycleObserver;
class CookieDeprecationLabel;
class Geolocation;
class HandwritingRecognitionService;
class Ink;
class MediaDevices;
class MediaSession;
class ModelContextSupplement;
class NavigatorAuction;
class NavigatorBeacon;
class NavigatorContentUtils;
class NavigatorDevicePosture;
class NavigatorGamepad;
class NavigatorKeyboard;
class NavigatorLogin;
class PageVisibilityObserver;
class NavigatorManagedData;
class NavigatorPlugins;
class NavigatorPreferences;
class NavigatorShare;
class NavigatorUserActivation;
class NavigatorWebInstall;
class NavigatorWebMIDI;
class Presentation;
class Scheduling;
class SubApps;
class VibrationController;
class VirtualKeyboard;
class WindowControlsOverlay;
class XRSystem;

class CORE_EXPORT Navigator final : public NavigatorBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Navigator(ExecutionContext*);

  // NavigatorCookies
  bool cookieEnabled() const;

  bool webdriver() const;

  String productSub() const;
  String vendor() const;
  String vendorSub() const;

  String platform() const override;

  String GetAcceptLanguages() override;
  void SetUserAgentMetadataForTesting(UserAgentMetadata);

  void Trace(Visitor*) const override;

  Geolocation* GetGeolocation() const { return geolocation_; }
  void SetGeolocation(Geolocation* geolocation) { geolocation_ = geolocation; }

  ModelContextSupplement* GetModelContextSupplement() const {
    return model_context_supplement_;
  }
  void SetModelContextSupplement(
      ModelContextSupplement* model_context_supplement) {
    model_context_supplement_ = model_context_supplement;
  }

  NavigatorPreferences* GetNavigatorPreferences() const {
    return navigator_preferences_;
  }
  void SetNavigatorPreferences(NavigatorPreferences* navigator_preferences) {
    navigator_preferences_ = navigator_preferences;
  }

  NavigatorUserActivation* GetNavigatorUserActivation() const {
    return navigator_user_activation_;
  }
  void SetNavigatorUserActivation(
      NavigatorUserActivation* navigator_user_activation) {
    navigator_user_activation_ = navigator_user_activation;
  }

  Scheduling* GetScheduling() const { return scheduling_; }
  void SetScheduling(Scheduling* scheduling) { scheduling_ = scheduling; }

  ForwardDeclaredMember<AuthenticationCredentialsContainer>
  GetAuthenticationCredentialsContainer() const {
    return authentication_credentials_container_;
  }
  void SetAuthenticationCredentialsContainer(
      ForwardDeclaredMember<AuthenticationCredentialsContainer>
          authentication_credentials_container) {
    authentication_credentials_container_ =
        authentication_credentials_container;
  }

  ForwardDeclaredMember<BatteryManager, ContextLifecycleObserver>
  GetBatteryManager() const {
    return battery_manager_;
  }
  void SetBatteryManager(
      ForwardDeclaredMember<BatteryManager, ContextLifecycleObserver>
          battery_manager) {
    battery_manager_ = battery_manager;
  }

  ForwardDeclaredMember<Bluetooth> GetBluetooth() const { return bluetooth_; }
  void SetBluetooth(ForwardDeclaredMember<Bluetooth> bluetooth) {
    bluetooth_ = bluetooth;
  }

  ForwardDeclaredMember<Clipboard> GetClipboard() const { return clipboard_; }
  void SetClipboard(ForwardDeclaredMember<Clipboard> clipboard) {
    clipboard_ = clipboard;
  }

  ForwardDeclaredMember<ContactsManager> GetContactsManager() const {
    return contacts_manager_;
  }
  void SetContactsManager(
      ForwardDeclaredMember<ContactsManager> contacts_manager) {
    contacts_manager_ = contacts_manager;
  }

  ForwardDeclaredMember<CookieDeprecationLabel> GetCookieDeprecationLabel()
      const {
    return cookie_deprecation_label_;
  }
  void SetCookieDeprecationLabel(
      ForwardDeclaredMember<CookieDeprecationLabel> cookie_deprecation_label) {
    cookie_deprecation_label_ = cookie_deprecation_label;
  }

  ForwardDeclaredMember<HandwritingRecognitionService>
  GetHandwritingRecognitionService() const {
    return handwriting_recognition_service_;
  }
  void SetHandwritingRecognitionService(
      ForwardDeclaredMember<HandwritingRecognitionService>
          handwriting_recognition_service) {
    handwriting_recognition_service_ = handwriting_recognition_service;
  }

  ForwardDeclaredMember<Ink> GetInk() const { return ink_; }
  void SetInk(ForwardDeclaredMember<Ink> ink) { ink_ = ink; }

  ForwardDeclaredMember<MediaDevices, ContextLifecycleObserver>
  GetMediaDevices() const {
    return media_devices_;
  }
  void SetMediaDevices(
      ForwardDeclaredMember<MediaDevices, ContextLifecycleObserver>
          media_devices) {
    media_devices_ = media_devices;
  }

  ForwardDeclaredMember<MediaSession> GetMediaSession() const {
    return media_session_;
  }
  void SetMediaSession(ForwardDeclaredMember<MediaSession> media_session) {
    media_session_ = media_session;
  }

  ForwardDeclaredMember<NavigatorAuction> GetNavigatorAuction() const {
    return navigator_auction_;
  }
  void SetNavigatorAuction(
      ForwardDeclaredMember<NavigatorAuction> navigator_auction) {
    navigator_auction_ = navigator_auction;
  }

  ForwardDeclaredMember<NavigatorBeacon> GetNavigatorBeacon() const {
    return navigator_beacon_;
  }
  void SetNavigatorBeacon(
      ForwardDeclaredMember<NavigatorBeacon> navigator_beacon) {
    navigator_beacon_ = navigator_beacon;
  }

  ForwardDeclaredMember<NavigatorContentUtils> GetNavigatorContentUtils()
      const {
    return navigator_content_utils_;
  }
  void SetNavigatorContentUtils(
      ForwardDeclaredMember<NavigatorContentUtils> navigator_content_utils) {
    navigator_content_utils_ = navigator_content_utils;
  }

  ForwardDeclaredMember<NavigatorDevicePosture> GetNavigatorDevicePosture()
      const {
    return navigator_device_posture_;
  }
  void SetNavigatorDevicePosture(
      ForwardDeclaredMember<NavigatorDevicePosture> navigator_device_posture) {
    navigator_device_posture_ = navigator_device_posture;
  }

  ForwardDeclaredMember<NavigatorGamepad, PageVisibilityObserver>
  GetNavigatorGamepad() const {
    return navigator_gamepad_;
  }
  void SetNavigatorGamepad(
      ForwardDeclaredMember<NavigatorGamepad, PageVisibilityObserver>
          navigator_gamepad) {
    navigator_gamepad_ = navigator_gamepad;
  }

  ForwardDeclaredMember<NavigatorKeyboard> GetNavigatorKeyboard() const {
    return navigator_keyboard_;
  }
  void SetNavigatorKeyboard(
      ForwardDeclaredMember<NavigatorKeyboard> navigator_keyboard) {
    navigator_keyboard_ = navigator_keyboard;
  }

  ForwardDeclaredMember<NavigatorLogin> GetNavigatorLogin() const {
    return navigator_login_;
  }
  void SetNavigatorLogin(
      ForwardDeclaredMember<NavigatorLogin> navigator_login) {
    navigator_login_ = navigator_login;
  }

  ForwardDeclaredMember<NavigatorManagedData> GetNavigatorManagedData() const {
    return navigator_managed_data_;
  }
  void SetNavigatorManagedData(
      ForwardDeclaredMember<NavigatorManagedData> navigator_managed_data) {
    navigator_managed_data_ = navigator_managed_data;
  }

  ForwardDeclaredMember<NavigatorPlugins> GetNavigatorPlugins() const {
    return navigator_plugins_;
  }
  void SetNavigatorPlugins(
      ForwardDeclaredMember<NavigatorPlugins> navigator_plugins) {
    navigator_plugins_ = navigator_plugins;
  }

  ForwardDeclaredMember<NavigatorShare> GetNavigatorShare() const {
    return navigator_share_;
  }
  void SetNavigatorShare(
      ForwardDeclaredMember<NavigatorShare> navigator_share) {
    navigator_share_ = navigator_share;
  }

  ForwardDeclaredMember<NavigatorWebInstall> GetNavigatorWebInstall() const {
    return navigator_web_install_;
  }
  void SetNavigatorWebInstall(
      ForwardDeclaredMember<NavigatorWebInstall> navigator_web_install) {
    navigator_web_install_ = navigator_web_install;
  }

  ForwardDeclaredMember<NavigatorWebMIDI> GetNavigatorWebMIDI() const {
    return navigator_web_midi_;
  }
  void SetNavigatorWebMIDI(
      ForwardDeclaredMember<NavigatorWebMIDI> navigator_web_midi) {
    navigator_web_midi_ = navigator_web_midi;
  }

  ForwardDeclaredMember<Presentation> GetPresentation() const {
    return presentation_;
  }
  void SetPresentation(ForwardDeclaredMember<Presentation> presentation) {
    presentation_ = presentation;
  }

  ForwardDeclaredMember<SubApps> GetSubApps() const { return sub_apps_; }
  void SetSubApps(ForwardDeclaredMember<SubApps> sub_apps) {
    sub_apps_ = sub_apps;
  }

  ForwardDeclaredMember<VibrationController, ContextLifecycleObserver>
  GetVibrationController() const {
    return vibration_controller_;
  }
  void SetVibrationController(
      ForwardDeclaredMember<VibrationController, ContextLifecycleObserver>
          vibration_controller) {
    vibration_controller_ = vibration_controller;
  }

  ForwardDeclaredMember<VirtualKeyboard> GetVirtualKeyboard() const {
    return virtual_keyboard_;
  }
  void SetVirtualKeyboard(
      ForwardDeclaredMember<VirtualKeyboard> virtual_keyboard) {
    virtual_keyboard_ = virtual_keyboard;
  }

  ForwardDeclaredMember<WindowControlsOverlay> GetWindowControlsOverlay()
      const {
    return window_controls_overlay_;
  }
  void SetWindowControlsOverlay(
      ForwardDeclaredMember<WindowControlsOverlay> window_controls_overlay) {
    window_controls_overlay_ = window_controls_overlay;
  }

  ForwardDeclaredMember<XRSystem, ContextLifecycleObserver> GetXRSystem()
      const {
    return xrsystem_;
  }
  void SetXRSystem(
      ForwardDeclaredMember<XRSystem, ContextLifecycleObserver> xrsystem) {
    xrsystem_ = xrsystem;
  }

 private:
  UserAgentMetadata metadata_;

  Member<Geolocation> geolocation_;
  Member<ModelContextSupplement> model_context_supplement_;
  Member<NavigatorPreferences> navigator_preferences_;
  Member<NavigatorUserActivation> navigator_user_activation_;
  Member<Scheduling> scheduling_;
  ForwardDeclaredMember<AuthenticationCredentialsContainer>
      authentication_credentials_container_;
  ForwardDeclaredMember<BatteryManager, ContextLifecycleObserver>
      battery_manager_;
  ForwardDeclaredMember<Bluetooth> bluetooth_;
  ForwardDeclaredMember<Clipboard> clipboard_;
  ForwardDeclaredMember<ContactsManager> contacts_manager_;
  ForwardDeclaredMember<CookieDeprecationLabel> cookie_deprecation_label_;
  ForwardDeclaredMember<HandwritingRecognitionService>
      handwriting_recognition_service_;
  ForwardDeclaredMember<Ink> ink_;
  ForwardDeclaredMember<MediaDevices, ContextLifecycleObserver> media_devices_;
  ForwardDeclaredMember<MediaSession> media_session_;
  ForwardDeclaredMember<NavigatorAuction> navigator_auction_;
  ForwardDeclaredMember<NavigatorBeacon> navigator_beacon_;
  ForwardDeclaredMember<NavigatorContentUtils> navigator_content_utils_;
  ForwardDeclaredMember<NavigatorDevicePosture> navigator_device_posture_;
  ForwardDeclaredMember<NavigatorGamepad, PageVisibilityObserver>
      navigator_gamepad_;
  ForwardDeclaredMember<NavigatorKeyboard> navigator_keyboard_;
  ForwardDeclaredMember<NavigatorLogin> navigator_login_;
  ForwardDeclaredMember<NavigatorManagedData> navigator_managed_data_;
  ForwardDeclaredMember<NavigatorPlugins> navigator_plugins_;
  ForwardDeclaredMember<NavigatorShare> navigator_share_;
  ForwardDeclaredMember<NavigatorWebInstall> navigator_web_install_;
  ForwardDeclaredMember<NavigatorWebMIDI> navigator_web_midi_;
  ForwardDeclaredMember<Presentation> presentation_;
  ForwardDeclaredMember<SubApps> sub_apps_;
  ForwardDeclaredMember<VibrationController, ContextLifecycleObserver>
      vibration_controller_;
  ForwardDeclaredMember<VirtualKeyboard> virtual_keyboard_;
  ForwardDeclaredMember<WindowControlsOverlay> window_controls_overlay_;
  ForwardDeclaredMember<XRSystem, ContextLifecycleObserver> xrsystem_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_H_
