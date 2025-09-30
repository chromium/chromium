# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import select
import time
from typing import Any, Callable, List, Optional

import dbus
from snegg.c.libei import libei
import snegg.ei as ei


class DBusService:
  def __init__(self, bus: dbus.Bus, bus_name: str, interface: str) -> None:
    self.bus = bus
    self.bus_name = bus_name
    self.interface = interface

  def call(self,
           path: str,
           method: str,
           signature: str = "",
           params: List[Any] = []) -> Any:
    return self.bus.call_blocking(self.bus_name, path, self.interface,
                                  method, signature, params)

  def get(self, path: str, namespace: str, property_name: str) -> Any:
    return self.bus.call_blocking(self.bus_name, path,
                                  "org.freedesktop.DBus.Properties", "Get",
                                  "ss", [namespace, property_name],
    )


class RemoteDesktop(DBusService):
  def __init__(self, bus: dbus.Bus) -> None:
    super().__init__(bus, "org.gnome.Mutter.RemoteDesktop",
                     "org.gnome.Mutter.RemoteDesktop")

  def create_session(self) -> "RemoteDesktopSession":
    path = self.call("/org/gnome/Mutter/RemoteDesktop", "CreateSession",
                     "b", [True])
    return RemoteDesktopSession(self.bus, path)


class Startable(DBusService):
  def __init__(self, bus: dbus.Bus, bus_name: str, path: str,
               interface: str) -> None:
    super().__init__(bus, bus_name, interface)
    self.path = path
    self.started = False

  def __del__(self) -> None:
    if self.started:
      try:
        self.call(self.path, "Stop")
      except:
        pass

  def start(self) -> None:
    self.call(self.path, "Start")
    self.started = True


class RemoteDesktopSession(Startable):
  def __init__(self, bus: dbus.Bus, path: str) -> None:
    super().__init__(bus, "org.gnome.Mutter.RemoteDesktop", path,
                     "org.gnome.Mutter.RemoteDesktop.Session",
    )

  def id(self) -> str:
    return self.get(self.path, "org.gnome.Mutter.RemoteDesktop.Session",
                    "SessionId")

  def connect_to_eis(self) -> Any:
    return self.call(self.path, "ConnectToEIS", "a{sv}", [{}])


class ScreenCast(DBusService):
  def __init__(self, bus: dbus.Bus) -> None:
    super().__init__(bus, "org.gnome.Mutter.ScreenCast",
                     "org.gnome.Mutter.ScreenCast")

  def create_session(self, session_id: str) -> "ScreenCastSession":
    path = self.call("/org/gnome/Mutter/ScreenCast", "CreateSession",
                     "a{sv}", [{"remote-desktop-session-id": session_id}],
    )
    return ScreenCastSession(self.bus, path)


class ScreenCastSession(DBusService):
  def __init__(self, bus: dbus.Bus, path: str) -> None:
    super().__init__(bus, "org.gnome.Mutter.ScreenCast",
                     "org.gnome.Mutter.ScreenCast.Session")
    self.path = path

  def record_monitor(self) -> "ScreenCastStream":
    path = self.call(self.path, "RecordMonitor", "sa{sv}", ["", {}])
    return ScreenCastStream(self.bus, path)


class ScreenCastStream(Startable):
  def __init__(self, bus: dbus.Bus, path: str) -> None:
    super().__init__(bus, "org.gnome.Mutter.ScreenCast", path,
                     "org.gnome.Mutter.ScreenCast.Stream",
    )


@dataclasses.dataclass
class Devices:
  pointer_relative: Optional[ei.Device] = None
  pointer_absolute: Optional[ei.Device] = None
  keyboard: Optional[ei.Device] = None
  scroll: Optional[ei.Device] = None
  button: Optional[ei.Device] = None

  def __del__(self) -> None:
    if self.pointer_relative:
      self.pointer_relative.stop_emulating()
    if self.pointer_absolute:
      self.pointer_absolute.stop_emulating()
    if self.keyboard:
      self.keyboard.stop_emulating()
    if self.scroll:
      self.scroll.stop_emulating()
    if self.button:
      self.button.stop_emulating()

  def ready(self) -> bool:
    # Devices are considered ready when all have been received. Although
    # invocations might not need them all, attempting to send input before
    # all have been received doesn't work reliably. Note that the script
    # might wait forever if some of these devices don't exist.
    return (self.pointer_relative and self.pointer_absolute
            and self.keyboard and self.scroll and self.button)


@dataclasses.dataclass
class IOLike:
  fd: int

  def fileno(self) -> int:
    return self.fd


@dataclasses.dataclass
class Options:
  executors: list[Callable[[Devices],
                           None]] = dataclasses.field(default_factory=list)


def connect_to_eis(fd: int, options: Options) -> None:
  ctx = ei.Sender.create_for_fd(fd=IOLike(fd), name="ei-debug-events")
  poll = select.poll()
  poll.register(ctx.fd)
  devices = Devices()
  while poll.poll():
    ctx.dispatch()
    for e in ctx.events:
      # Protect access to event_type because the getter throws for unknown
      # events.
      try:
        event_type = e.event_type
      except Exception as err:
        print(err)
        continue
      if options.verbose:
        print(e)

      if event_type == ei.EventType.SEAT_ADDED:
        if options.verbose:
          print(e.seat)
        e.seat.bind(ei.DeviceCapability.all())
      elif event_type == ei.EventType.DEVICE_RESUMED:
        if options.verbose:
          print(e.device)
        e.device.start_emulating()
        if ei.DeviceCapability.POINTER in e.device.capabilities:
          devices.pointer_relative = e.device
        if ei.DeviceCapability.POINTER_ABSOLUTE in e.device.capabilities:
          devices.pointer_absolute = e.device
        if ei.DeviceCapability.SCROLL in e.device.capabilities:
          devices.scroll = e.device
        if ei.DeviceCapability.BUTTON in e.device.capabilities:
          devices.button = e.device
        if ei.DeviceCapability.KEYBOARD in e.device.capabilities:
          devices.keyboard = e.device
        if devices.ready():
          for executor in options.executors:
            executor(devices)
          return


# snegg doesn't expose the scroll functions on devices
def scroll_discrete(device: ei.Device, y: int) -> None:
  libei.device_scroll_discrete(device._cobject, 0, y)  # pylint: disable=protected-access
  device.frame()


def scroll_delta(device: ei.Device, y: int) -> None:
  # For a virtual device (which we're certainly dealing with in this script),
  # y is in dips. For a physical device, it would be mm.
  libei.device_scroll_delta(device._cobject, 0, y)  # pylint: disable=protected-access
  device.frame()


class SleepAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(lambda _: time.sleep(values[0]))


class ScrollDeltaAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(lambda d: scroll_delta(d.scroll, values[0]))


class ScrollDiscreteAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(
      lambda d: scroll_discrete(d.scroll, 120 * values[0]))


class MoveToAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(
      lambda d: d.pointer_absolute.pointer_motion_absolute(
          values[0], values[1]).frame())


class MoveByAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(lambda d: d.pointer_relative.pointer_motion(
        values[0], values[1]).frame())


class ClickAction(argparse.Action):
  BTN_LEFT = 0x110  # Per linux/input-event-codes.h
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               value: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(
      lambda d: d.button.button_button(value + self.BTN_LEFT, True).frame(
      ).button_button(value + self.BTN_LEFT, False).frame())


class TypeAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(lambda d: TypeAction.exec(d.keyboard, values))

  @staticmethod
  def exec(keyboard: ei.Device, values: List[int]) -> None:
    for value in values:
      keyboard.keyboard_key(value, True).frame().keyboard_key(
          value, False).frame()


class KeyDownAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(
        lambda d: d.keyboard.keyboard_key(values[0], True).frame())


class KeyUpAction(argparse.Action):
  def __call__(self,
               parser: argparse.ArgumentParser,
               namespace: argparse.Namespace,
               values: Any,
               option: Optional[str] = None) -> None:
    namespace.executors.append(
        lambda d: d.keyboard.keyboard_key(values[0], False).frame())


if __name__ == "__main__":
  options = Options()
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "-v", "--verbose",
      action="store_true",
      help="enable debug output",
  )
  parser.add_argument(
      "--sleep",
      action=SleepAction,
      nargs=1,
      type=float,
      metavar="seconds",
      help="sleep before executing the next command.",
  )
  parser.add_argument(
      "--scroll_delta",
      action=ScrollDeltaAction,
      nargs=1,
      type=int,
      metavar="px",
      help="scroll in pixels down (+ve) or down (-ve)",
  )
  parser.add_argument(
      "--scroll_discrete",
      action=ScrollDiscreteAction,
      nargs=1,
      type=int,
      metavar="ticks",
      help="scroll in ticks down (+ve) or down (-ve)",
  )
  parser.add_argument(
      "--click",
      action=ClickAction,
      nargs="?",
      const=0,  # Left button
      type=int,
      metavar="button",
      help="click the mouse (left button by default)",
  )
  parser.add_argument(
      "--move_to",
      action=MoveToAction,
      nargs=2,
      type=float,
      metavar=("x", "y"),
      help="move the mouse to a location",
  )
  parser.add_argument(
      "--move_by",
      action=MoveByAction,
      nargs=2,
      type=float,
      metavar=("dx", "dy"),
      help="move the mouse by an amount",
  )
  parser.add_argument(
      "--type",
      action=TypeAction,
      nargs="+",
      type=int,
      metavar="keycode",
      help="type (press and release) a sequence of keys",
  )
  parser.add_argument(
      "--key_down",
      action=KeyDownAction,
      nargs=1,
      type=int,
      metavar="keycode",
      help="press a key",
  )
  parser.add_argument(
      "--key_up",
      action=KeyUpAction,
      nargs=1,
      type=int,
      metavar="keycode",
      help="release a key",
  )
  args = parser.parse_args(namespace=options)

  if not options.executors:
    parser.error("No commands specified")

  try:
    session_bus = dbus.SessionBus()
    remote_desktop_session = RemoteDesktop(session_bus).create_session()
    session_id = remote_desktop_session.id()
    eis_fd = remote_desktop_session.connect_to_eis()
    screencast_session = ScreenCast(session_bus).create_session(session_id)
    remote_desktop_session.start()
    stream = screencast_session.record_monitor()
    stream.start()
    connect_to_eis(eis_fd.take(), options)
  except KeyboardInterrupt:
    pass
