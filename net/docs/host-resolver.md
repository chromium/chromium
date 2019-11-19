# Host Resolution

This document is a brief overview of how host resolution works in the Chrome
network stack.

The stack includes two different implementations: the "system" or "platform"
resolver, and the "async" or "built-in" resolver. The higher layers are shared
between the implementations, and the lower layers are implemented separately.

## Shared layers

### Host resolver

The HostResolverImpl is the main interface between DNS and the rest of the
network stack. It checks the HostCache, checks if there is an already running
Job, and schedules a new Job if there isn't one in progress.

Data collected at this layer:
* "Net.DNS.TotalTime" (recommended for DNS experiments)
* "Net.DNS.TotalTimeNotCached"

### Job

The HostResolverImpl::Job represents a single DNS resolution from the network
(or in some cases the OS's DNS cache, which Chrome doesn't know about). It
starts a task depending on which implementation should be used. If a DnsTask
fails, it retries using a ProcTask.

Data collected at this layer:
* "Net.DNS.ResolveSuccessTime" (also by address family)
* "Net.DNS.ResolveFailureTime" (also by address family)
* "Net.DNS.ResolveCategory"
* "Net.DNS.ResolveError.Fast"
* "Net.DNS.ResolveError.Slow"

## System resolver

### Task

The entry point for the system resolver is HostResolverImpl::ProcTask. The task
runs almost entirely on ThreadPool. Its main implementation is in
SystemHostResolverProc. Other implementations of HostResolverProc can be swapped
in for testing.

Data collected at this layer:
* "Net.DNS.ProcTask.SuccessTime"
* "Net.DNS.ProcTask.FailureTime"
* "Net.OSErrorsForGetaddrinfo*"

### Attempt

Attempts in the system resolver are not a separate class. They're implemented as
separate tasks posted to ThreadPool.

Data collected at this layer:
* "DNS.AttemptFirstSuccess"
* "DNS.AttemptFirstFailure"
* "DNS.AttemptSuccess"
* "DNS.AttemptFailure"
* "DNS.AttemptDiscarded"
* "DNS.AttemptCancelled"
* "DNS.AttemptSuccessDuration"
* "DNS.AttemptFailDuration"

## Async resolver

### Task

The entry point for the async resolver is HostResolverImpl::DnsTask. DnsTask
starts one DnsTransaction for each lookup needed, which can be one for a single
address family or two when both A and AAAA are needed.

Data collected at this layer:
* "Net.DNS.DnsTask.SuccessTime"
* "Net.DNS.DnsTask.FailureTime"
* "Net.DNS.DnsTask.ErrorBeforeFallback.Fast"
* "Net.DNS.DnsTask.ErrorBeforeFallback.Slow"
* "Net.DNS.DnsTask.Errors"

### Transaction

The main implementation of the async resolver is in the DnsTransaction. Each
transaction represents a single query, which might be tried multiple times or in
different ways.

### Attempt

Attempts in the async resolver are an explicit layer, implemented by subclasses
of DnsAttempt. In most cases, DnsUDPAttempt is used. DnsTCPAttempt is used
instead when the server requests it. DnsHTTPAttempt is experimental.
