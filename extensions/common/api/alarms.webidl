// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary Alarm {
  // Name of this alarm.
  required DOMString name;

  // Time at which this alarm was scheduled to fire, in milliseconds past the
  // epoch (e.g. <code>Date.now() + n</code>).  For performance reasons, the
  // alarm may have been delayed an arbitrary amount beyond this.
  required double scheduledTime;

  // If not null, the alarm is a repeating alarm and will fire again in
  // <var>periodInMinutes</var> minutes.
  double periodInMinutes;
};

// TODO(mpcomplete): rename to CreateInfo when http://crbug.com/123073 is
// fixed.
dictionary AlarmCreateInfo {
  // Time at which the alarm should fire, in milliseconds past the epoch
  // (e.g. <code>Date.now() + n</code>).
  double when;

  // Length of time in minutes after which the <code>onAlarm</code> event
  // should fire.
  //
  // <!-- TODO: need minimum=0 -->
  double delayInMinutes;

  // If set, the onAlarm event should fire every <var>periodInMinutes</var>
  // minutes after the initial event specified by <var>when</var> or
  // <var>delayInMinutes</var>.  If not set, the alarm will only fire once.
  //
  // <!-- TODO: need minimum=0 -->
  double periodInMinutes;
};

// Listener callback for the onAlarm event.
// |alarm|: The alarm that has elapsed.
callback OnAlarmListener = undefined (Alarm alarm);

interface OnAlarmEvent : ExtensionEvent {
  static undefined addListener(OnAlarmListener listener);
  static undefined removeListener(OnAlarmListener listener);
  static boolean hasListener(OnAlarmListener listener);
};

// Use the <code>chrome.alarms</code> API to schedule code to run
// periodically or at a specified time in the future.
interface Alarms {
  // Creates an alarm.  Near the time(s) specified by <var>alarmInfo</var>,
  // the <code>onAlarm</code> event is fired. If there is another alarm with
  // the same name (or no name if none is specified), it will be cancelled and
  // replaced by this alarm.
  //
  // In order to reduce the load on the user's machine, Chrome limits alarms
  // to at most once every 30 seconds but may delay them an arbitrary amount
  // more.  That is, setting <code>delayInMinutes</code> or
  // <code>periodInMinutes</code> to less than <code>0.5</code> will not be
  // honored and will cause a warning.  <code>when</code> can be set to less
  // than 30 seconds after "now" without warning but won't actually cause the
  // alarm to fire for at least 30 seconds.
  //
  // To help you debug your app or extension, when you've loaded it unpacked,
  // there's no limit to how often the alarm can fire.
  //
  // |name|: Optional name to identify this alarm. Defaults to the empty
  // string.
  // |alarmInfo|: Describes when the alarm should fire.  The initial time must
  // be specified by either <var>when</var> or <var>delayInMinutes</var> (but
  // not both).  If <var>periodInMinutes</var> is set, the alarm will repeat
  // every <var>periodInMinutes</var> minutes after the initial event.  If
  // neither <var>when</var> or <var>delayInMinutes</var> is set for a
  // repeating alarm, <var>periodInMinutes</var> is used as the default for
  // <var>delayInMinutes</var>.
  // |Returns|: Invoked when the alarm has been created.
  static Promise<undefined> create(
      optional DOMString name,
      AlarmCreateInfo alarmInfo);

  // Retrieves details about the specified alarm.
  // |name|: The name of the alarm to get. Defaults to the empty string.
  // |PromiseValue|: alarm
  [requiredCallback] static Promise<Alarm?> get(optional DOMString name);

  // Gets an array of all the alarms.
  // |PromiseValue|: alarms
  [requiredCallback] static Promise<sequence<Alarm>> getAll();

  // Clears the alarm with the given name.
  // |name|: The name of the alarm to clear. Defaults to the empty string.
  // |PromiseValue|: wasCleared
  static Promise<boolean> clear(optional DOMString name);

  // Clears all alarms.
  // |PromiseValue|: wasCleared
  static Promise<boolean> clearAll();

  // Fired when an alarm has elapsed. Useful for event pages.
  static attribute OnAlarmEvent onAlarm;
};

partial interface Browser {
  static attribute Alarms alarms;
};
