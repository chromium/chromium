# Network Service Task Scheduler

This directory contains the implementation for an experimental task scheduler on
Chromium's Network Service.

The scheduler introduces a `base::sequence_manager::SequenceManager` configured
with multiple prioritized task queues. The primary goal is to allow
high-priority network tasks (e.g., those critical for navigation) to execute
with precedence over lower-priority tasks, aiming to improve user-perceived
performance metrics like FCP and LCP.

## Design

For a detailed explanation of the design, motivations, and implementation plan,
please refer to the design document:
[go/task-scheduler-in-net](http://go/task-scheduler-in-net) (Google internal)

## Overview

The core mechanism involves:

1. Initializing a `SequenceManager` on the Network Service Thread.
2. Creating at least two `TaskQueue`s: one for high-priority tasks and one for
   default-priority tasks.
3. Exposing `TaskRunner`s associated with these queues.
4. Modifying relevant `PostTask` call sites in `//net` and `//services/network`
   to route tasks to the appropriate `TaskRunner` based on the task's conceptual
   priority (often derived from `net::RequestPriority`).

## Status

This feature is currently experimental and under development. Its impact is
being evaluated via diagnostic metrics and Finch experiments.
