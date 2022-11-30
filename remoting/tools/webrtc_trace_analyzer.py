#!/usr/bin/env python3
#
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage:
# $ CHROME_REMOTE_DESKTOP_HOST_EXTRA_PARAMS="--webrtc-trace-event-file=/tmp/crd-webrtc-trace" ./out/Default/remoting/chrome-remote-desktop --start -f --child-process 2>/tmp/crd.log
# <kill chrome-remote-desktop with a SIGTERM, it must shut down cleanly or the json file will be truncated>
# $ remoting/tools/webrtc_trace_analyzer.py /tmp/crd-webrtc-trace | less

import dataclasses
import enum
import json
import pprint
import sys
from typing import Any, Dict, Iterable, List, Optional


class Phase(enum.Enum):
  BEGIN = 'B'
  END = 'E'
  INSTANT = 'I'
  ASYNC_BEGIN = 'S'
  ASYNC_STEP = 'T'
  ASYNC_END = 'F'
  FLOW_BEGIN = 's'
  FLOW_STEP = 't'
  FLOW_END = 'f'
  METADATA = 'M'
  COUNTER = 'C'


@dataclasses.dataclass
class Event:
  name: str
  cat: str  # category, usually (always?) "webrtc"
  ts: int  # CLOCK_MONOTONIC, microseconds since machine boot
  duration: int  # microseconds
  pid: int
  tid: int
  args: Dict[str, Any]


@dataclasses.dataclass
class VideoSendEvent(Event):
  time_since_last: Optional[int] = None

  @classmethod
  def from_event(cls, event: Event, time_since_last: Optional[int] = None):
    return cls(**dataclasses.asdict(event),
               time_since_last=time_since_last)


def analyzer_video(events: Iterable[Event]):
  last_send_ts_dict = {}
  for event in events:
    event_name = event.args.get('step')
    last_send_ts = last_send_ts_dict.get(event_name)
    if last_send_ts is None:
      time_since_last = None
    else:
      time_since_last = event.ts - last_send_ts
    last_send_ts_dict[event_name] = event.ts

    yield VideoSendEvent.from_event(event, time_since_last=time_since_last)


ANALYZER_FUNCTIONS = {
  'Video': analyzer_video
}


def analyze_events(events: Iterable[Dict[str, Any]]):
  analyzed_events: Dict[str, List[Any]] = {}
  for event in events:
    analyzed_event = Event(name=event['name'],
                           cat=event['cat'],
                           ts=event['ts'],
                           duration=0,
                           pid=event['pid'],
                           tid=event['tid'],
                           args=event.get('args', {}))
    if Phase(event['ph']) == Phase.END:
      prev_analyzed_event = analyzed_events[analyzed_event.name][-1]
      prev_analyzed_event.duration = analyzed_event.ts - prev_analyzed_event.ts
    else:
      analyzed_events.setdefault(analyzed_event.name, []).append(
        analyzed_event)

  for name, items in analyzed_events.items():
    analyzed_events[name] = list(
      ANALYZER_FUNCTIONS.get(name, lambda x: x)(items))

  return analyzed_events


def main(argv: List[str]):
  if len(argv) != 2:
    print(f'Usage: {argv[0]} <trace file>')
    sys.exit(2)

  with open(argv[1], 'r') as trace_file:
    trace_data = json.load(trace_file)

  analyzed_events = analyze_events(trace_data['traceEvents'])
  pp = pprint.PrettyPrinter(indent=2)
  pp.pprint(analyzed_events)


if __name__ == '__main__':
  main(sys.argv)
