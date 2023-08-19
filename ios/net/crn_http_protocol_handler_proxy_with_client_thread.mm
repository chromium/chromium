// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_protocol_handler_proxy_with_client_thread.h"

#include <stddef.h>

#include "base/check.h"
#include "base/time/time.h"
#import "ios/net/protocol_handler_util.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

// When the protocol is invalidated, no synchronization (lock) is needed:
// - The actual calls to the protocol and its invalidation are all done on
//   clientThread_ and thus are serialized.
// - When a proxy method is called, the protocol is compared to nil. There may
//   be a conflict at this point, in the case the protocol is being invalidated
//   during this comparison. However, in such a case, the actual value of the
//   pointer does not matter: an invalid pointer will behave as a valid one and
//   post a task on the clientThread_, and that task will be handled correctly,
//   as described by the item above.

@interface CRNHTTPProtocolHandlerProxyWithClientThread () {
  __weak NSURLProtocol* _protocol;
  // Thread used to call the client back.
  // This thread does not have a base::MessageLoop, and thus does not work with
  // the usual task posting functions.
  __weak NSThread* _clientThread;
  // The run loop modes to use when posting tasks to |clientThread_|.
  NSArray* _runLoopModes;
  // The request URL.
  NSString* _url;
  // The creation time of the request.
  base::Time _creationTime;
  // |requestComplete_| is used in debug to check that the client is not called
  // after completion.
  BOOL _requestComplete;
  BOOL _paused;

  // Contains code blocks to execute when the connection transitions from paused
  // to resumed state.
  NSMutableArray<void (^)()>* _queuedBlocks;
}

// Performs queued blocks on |clientThread_| using |runLoopModes_|.
- (void)runQueuedBlocksOnClientThread;
// These functions are just wrappers around the corresponding
// NSURLProtocolClient methods, used for task posting.
- (void)didFailWithErrorOnClientThread:(NSError*)error;
- (void)didLoadDataOnClientThread:(NSData*)data;
- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response;
- (void)wasRedirectedToRequestOnClientThread:(NSURLRequest*)request
                            redirectResponse:(NSURLResponse*)response;
- (void)didFinishLoadingOnClientThread;
@end

@implementation CRNHTTPProtocolHandlerProxyWithClientThread

- (instancetype)initWithProtocol:(NSURLProtocol*)protocol
                    clientThread:(NSThread*)clientThread
                     runLoopMode:(NSString*)mode {
  DCHECK(protocol);
  DCHECK(clientThread);
  if ((self = [super init])) {
    _protocol = protocol;
    _url = [[[[protocol request] URL] absoluteString] copy];
    _creationTime = base::Time::Now();
    _clientThread = clientThread;
    // Use the common run loop mode in addition to the client thread mode, in
    // hope that our tasks are executed even if the client thread changes mode
    // later on.
    if ([mode isEqualToString:NSRunLoopCommonModes])
      _runLoopModes = @[ NSRunLoopCommonModes ];
    else
      _runLoopModes = @[ mode, NSRunLoopCommonModes ];
    _queuedBlocks = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)invalidate {
  DCHECK([NSThread currentThread] == _clientThread);
  _protocol = nil;
  _requestComplete = YES;
  // Note that there may still be queued blocks here, if the chrome network
  // stack continues to emit events after the system network stack has paused
  // the request, and then the system network stack destroys the request.
  _queuedBlocks = nil;
}

- (void)runQueuedBlocksOnClientThread {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  // Each of the queued blocks may cause the system network stack to pause
  // this request, in which case |runQueuedBlocksOnClientThread| should
  // immediately stop running further queued invocations. The queue will be
  // drained again the next time the system network stack calls |resume|.
  //
  // Specifically, the system stack can call back into |pause| with this
  // function still on the call stack. However, since new blocks are
  // enqueued on this thread via posted invocations, no new blocks can be
  // added while this function is running.
  while (!_paused && _queuedBlocks.count > 0) {
    void (^block)() = _queuedBlocks[0];
    // Since |_queuedBlocks| owns the only reference to each queued
    // block, this function has to retain another reference before removing
    // the queued block from the array.
    block();
    [_queuedBlocks removeObjectAtIndex:0];
  }
}

- (void)postBlockToClientThread:(dispatch_block_t)block {
  DCHECK(block);
  [self performSelector:@selector(performBlockOnClientThread:)
               onThread:_clientThread
             withObject:[block copy]
          waitUntilDone:NO
                  modes:_runLoopModes];
}

- (void)performBlockOnClientThread:(dispatch_block_t)block {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  DCHECK(block);
  if (!_paused) {
    block();
  } else {
    [_queuedBlocks addObject:block];
  }
}

#pragma mark Proxy methods called from any thread.

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  NSError* error =
      net::GetIOSError(nsErrorCode, netErrorCode, _url, _creationTime);
  [self postBlockToClientThread:^{
    [self didFailWithErrorOnClientThread:error];
  }];
}

- (void)didLoadData:(NSData*)data {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postBlockToClientThread:^{
    [self didLoadDataOnClientThread:data];
  }];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postBlockToClientThread:^{
    [self didReceiveResponseOnClientThread:response];
  }];
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postBlockToClientThread:^{
    [self wasRedirectedToRequestOnClientThread:request
                              redirectResponse:redirectResponse];
  }];
}

- (void)didFinishLoading {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postBlockToClientThread:^{
    [self didFinishLoadingOnClientThread];
  }];
}

// Feature support methods that don't forward to the NSURLProtocolClient.
- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest {
  // no-op.
}

#pragma mark Proxy methods called from the client thread.

- (void)didFailWithErrorOnClientThread:(NSError*)error {
  _requestComplete = YES;
  [[_protocol client] URLProtocol:_protocol didFailWithError:error];
}

- (void)didLoadDataOnClientThread:(NSData*)data {
  [[_protocol client] URLProtocol:_protocol didLoadData:data];
}

- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response {
  [[_protocol client] URLProtocol:_protocol
               didReceiveResponse:response
               cacheStoragePolicy:NSURLCacheStorageNotAllowed];
}

- (void)wasRedirectedToRequestOnClientThread:(NSURLRequest*)request
                            redirectResponse:(NSURLResponse*)redirectResponse {
  [[_protocol client] URLProtocol:_protocol
           wasRedirectedToRequest:request
                 redirectResponse:redirectResponse];
}

- (void)didFinishLoadingOnClientThread {
  _requestComplete = YES;
  [[_protocol client] URLProtocolDidFinishLoading:_protocol];
}

- (void)pause {
  DCHECK([NSThread currentThread] == _clientThread);
  // It's legal (in fact, required) for |pause| to be called after the request
  // has already finished, so the usual invalidation DCHECK is missing here.
  _paused = YES;
}

- (void)resume {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  _paused = NO;
  [self runQueuedBlocksOnClientThread];
}

@end
