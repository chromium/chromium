// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of functions that are shared between ReadableStream and
// WritableStream.

(function(global, binding, v8) {
  'use strict';

  // Common private symbols. These correspond directly to internal slots in the
  // standard. "[[X]]" in the standard is spelt _X here.
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _queueTotalSize = v8.createPrivateSymbol('[[queueTotalSize]]');

  // Javascript functions. It is important to use these copies for security and
  // robustness. See "V8 Extras Design Doc", section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const Boolean = global.Boolean;
  const Number = global.Number;
  const Number_isFinite = Number.isFinite;
  const Number_isNaN = Number.isNaN;

  const RangeError = global.RangeError;
  const TypeError = global.TypeError;
  const TypeError_prototype = TypeError.prototype;

  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);
  const getPrototypeOf = global.Object.getPrototypeOf.bind(global.Object);
  const getOwnPropertyDescriptor =
        global.Object.getOwnPropertyDescriptor.bind(global.Object);

  const thenPromise = v8.uncurryThis(Promise.prototype.then);

  const JSON_parse = global.JSON.parse.bind(global.JSON);
  const JSON_stringify = global.JSON.stringify.bind(global.JSON);

  function hasOwnPropertyNoThrow(x, property) {
    // The cast of |x| to Boolean will eliminate undefined and null, which would
    // cause hasOwnProperty to throw a TypeError, as well as some other values
    // that can't be objects and so will fail the check anyway.
    return Boolean(x) && hasOwnProperty(x, property);
  }

  //
  // Assert is not normally enabled, to avoid the space and time overhead. To
  // enable, uncomment this definition and then in the file you wish to enable
  // asserts for, uncomment the assert statements and add this definition:
  // const assert = pred => binding.SimpleAssert(pred);
  //
  // binding.SimpleAssert = pred => {
  //   if (pred) {
  //     return;
  //   }
  //   v8.log('\n\n\n  *** ASSERTION FAILURE ***\n\n');
  //   v8.logStackTrace();
  //   v8.log('**************************************************\n\n');
  //   class StreamsAssertionError extends Error {}
  //   throw new StreamsAssertionError('Streams Assertion Failure');
  // };

  //
  // Promise-manipulation functions
  //

  // Not exported.
  function streamInternalError() {
    throw new RangeError('Stream API Internal Error');
  }

  function rejectPromise(p, reason) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.rejectPromise(p, reason);
  }

  function resolvePromise(p, value) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.resolvePromise(p, value);
  }

  function markPromiseAsHandled(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.markPromiseAsHandled(p);
  }

  function promiseState(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    return v8.promiseState(p);
  }

  //
  // Queue-with-Sizes Operations
  //
  function DequeueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].shift();
    container[_queueTotalSize] -= pair.size;
    if (container[_queueTotalSize] < 0) {
      container[_queueTotalSize] = 0;
    }
    return pair.value;
  }

  function EnqueueValueWithSize(container, value, size) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal 'slots.');
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(binding.streamErrors.invalidSize);
    }

    container[_queue].push({value, size});
    container[_queueTotalSize] += size;
  }

  function PeekQueueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].peek();
    return pair.value;
  }

  function ResetQueue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    container[_queue] = new binding.SimpleQueue();
    container[_queueTotalSize] = 0;
  }

  // Not exported.
  function IsFiniteNonNegativeNumber(v) {
    return Number_isFinite(v) && v >= 0;
  }

  function ValidateAndNormalizeHighWaterMark(highWaterMark) {
    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }
    return highWaterMark;
  }

  // Unlike the version in the standard, this implementation returns the
  // original function as-is if it is set. This means users of the return value
  // need to be careful to explicitly set |this| when calling it.
  function MakeSizeAlgorithmFromSizeFunction(size) {
    if (size === undefined) {
      return () => 1;
    }

    if (typeof size !== 'function') {
      throw new TypeError(binding.streamErrors.sizeNotAFunction);
    }

    return size;
  }

  //
  // Invoking functions.
  // These differ from the Invoke versions in the spec in that they take a fixed
  // number of arguments rather than a list, and also take a name to be used for
  // the function on error.
  //

  // Internal utility functions. Not exported.
  const callFunction = v8.uncurryThis(global.Function.prototype.call);
  const errTmplMustBeFunctionOrUndefined = name =>
      `${name} must be a function or undefined`;
  const Promise_resolve = Promise.resolve.bind(Promise);
  const Promise_reject = Promise.reject.bind(Promise);
  const Function_bind = v8.uncurryThis(global.Function.prototype.bind);

  function resolveMethod(O, P, nameForError) {
    const method = O[P];

    if (typeof method !== 'function' && typeof method !== 'undefined') {
      throw new TypeError(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    return method;
  }

  function CreateAlgorithmFromUnderlyingMethod(
      underlyingObject, methodName, algoArgCount, methodNameForError) {
    // assert(underlyingObject !== undefined,
    //        'underlyingObject is not undefined.');
    // assert(IsPropertyKey(methodName),
    // '! IsPropertyKey(methodName) is true.');
    // assert(algoArgCount === 0 || algoArgCount === 1,
    // 'algoArgCount is 0 or 1.');
    // assert(
    //     typeof methodNameForError === 'string',
    //     'methodNameForError is a string');
    const method =
        resolveMethod(underlyingObject, methodName, methodNameForError);
    // The implementation uses bound functions rather than lambdas where
    // possible to give the compiler the maximum opportunity to optimise.
    if (method === undefined) {
      return () => Promise_resolve();
    }

    if (algoArgCount === 0) {
      return Function_bind(PromiseCall0, undefined, method, underlyingObject);
    }

    return Function_bind(PromiseCall1, undefined, method, underlyingObject);
  }

  function CreateAlgorithmFromUnderlyingMethodPassingController(
      underlyingObject, methodName, algoArgCount, controller,
      methodNameForError) {
    // assert(underlyingObject !== undefined,
    //        'underlyingObject is not undefined.');
    // assert(IsPropertyKey(methodName),
    // '! IsPropertyKey(methodName) is true.');
    // assert(algoArgCount === 0 || algoArgCount === 1,
    // 'algoArgCount is 0 or 1.');
    // assert(typeof controller === 'object', 'controller is an object');
    // assert(
    //     typeof methodNameForError === 'string',
    //     'methodNameForError is a string');
    const method =
        resolveMethod(underlyingObject, methodName, methodNameForError);
    if (method === undefined) {
      return () => Promise_resolve();
    }

    if (algoArgCount === 0) {
      return Function_bind(
          PromiseCall1, undefined, method, underlyingObject, controller);
    }

    return arg => PromiseCall2(method, underlyingObject, arg, controller);
  }

  // Modified from InvokeOrNoop in spec. Takes 1 argument.
  function CallOrNoop1(O, P, arg0, nameForError) {
    const method = resolveMethod(O, P, nameForError);
    if (method === undefined) {
      return undefined;
    }

    return callFunction(method, O, arg0);
  }

  function PromiseCall0(F, V) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function PromiseCall1(F, V, arg0) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V, arg0));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function PromiseCall2(F, V, arg0, arg1) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V, arg0, arg1));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  // Functions for transferable streams. See design doc
  // https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit

  const kPull = 1;
  const kCancel = 2;
  const kChunk = 3;
  const kClose = 4;
  const kAbort = 5;
  const kError = 6;

  function isATypeError(object) {
    // There doesn't appear to be a 100% reliable way to identify a TypeError
    // from JS.
    return getPrototypeOf(object) === TypeError_prototype;
  }

  function isADOMException(object) {
    try {
      callFunction(binding.DOMException_name_get, object);
      return true;
    } catch (e) {
      return false;
    }
  }

  // We'd like to able to transfer TypeError exceptions, but we can't, so we
  // hack around it. packReason() is guaranteed not to throw and the object
  // produced is guaranteed to be serializable by postMessage().
  function packReason(reason) {
    switch (typeof reason) {
      case 'string':
      case 'number':
        return {encoder: 'json', string: JSON_stringify(reason)};

      case 'object':
        try {
          if (isATypeError(reason)) {
            // "message" on TypeError is a normal property, meaning that if it
            // is set, it is set on the object itself. We can take advantage of
            // this to avoid executing user JavaScript in the case when the
            // TypeError was generated internally.
            let message;
            const descriptor = getOwnPropertyDescriptor(reason, 'message');
            if (descriptor) {
              message = descriptor.value;
              if (typeof message !== 'string') {
                message = undefined;
              }
            }
            return {encoder: 'typeerror', string: message};
          }

          if (isADOMException(reason)) {
            const message =
                  callFunction(binding.DOMException_message_get, reason);
            const name = callFunction(binding.DOMException_name_get, reason);
            return {
              encoder: 'domexception',
              string: JSON_stringify({message, name})
            };
          }

          // JSON_stringify() is lossy, but it will serialise things that
          // postMessage() won't.
          return {encoder: 'json', string: JSON_stringify(reason)};
        } catch (e) {
          return {encoder: 'typeerror', string: 'Cannot transfer message'};
        }

      default:
        return {encoder: 'undefined', string: undefined};
    }
  }

  function unpackReason(packedReason) {
    const {encoder, string} = packedReason;
    switch (encoder) {
      case 'json':
        return JSON_parse(string);

      case 'typeerror':
        return new TypeError(string);

      case 'domexception':
        const {message, name} = JSON_parse(string);
        return new binding.DOMException(message, name);

      case 'undefined':
        return undefined;
    }
  }

  function CreateCrossRealmTransformWritable(port) {
    let backpressurePromise = v8.createPromise();

    callFunction(binding.EventTarget_addEventListener, port, 'message', evt => {
      const {type, value} = callFunction(binding.MessageEvent_data_get, evt);
      // assert(type === kPull || type === kCancel || type === kError);
      switch (type) {
        case kPull:
          // assert(backPressurePromise !== undefined);
          resolvePromise(backpressurePromise);
          backpressurePromise = undefined;
          break;

        case kCancel:
        case kError:
          binding.WritableStreamDefaultControllerErrorIfNeeded(
              controller, unpackReason(value));
          if (backpressurePromise !== undefined) {
            resolvePromise(backpressurePromise);
            backpressurePromise = undefined;
          }
          break;
      }
    });

    callFunction(
        binding.EventTarget_addEventListener, port, 'messageerror', () => {
          const error = new binding.DOMException('chunk could not be cloned',
                                                 'DataCloneError');
          callFunction(binding.MessagePort_postMessage, port,
                       {type: kError, value: packReason(error)});
          callFunction(binding.MessagePort_close, port);
          binding.WritableStreamDefaultControllerErrorIfNeeded(controller,
                                                               error);
        });

    callFunction(binding.MessagePort_start, port);

    function doWrite(chunk) {
      backpressurePromise = v8.createPromise();
      try {
        callFunction(
            binding.MessagePort_postMessage, port,
            {type: kChunk, value: chunk});
      } catch (e) {
        callFunction(
            binding.MessagePort_postMessage, port,
            {type: kError, value: packReason(e)});
        callFunction(binding.MessagePort_close, port);
        throw e;
      }
    }

    const stream = binding.CreateWritableStream(
        () => undefined,
        chunk => {
          if (!backpressurePromise) {
            return PromiseCall1(doWrite, null, chunk);
          }
          return thenPromise(backpressurePromise, () => doWrite(chunk));
        },
        () => {
          callFunction(
              binding.MessagePort_postMessage, port,
              {type: kClose, value: undefined});
          callFunction(binding.MessagePort_close, port);
          return Promise_resolve();
        },
        reason => {
          callFunction(
              binding.MessagePort_postMessage, port,
              {type: kAbort, value: packReason(reason)});
          callFunction(binding.MessagePort_close, port);
          return Promise_resolve();
        });

    const controller = binding.getWritableStreamController(stream);
    return stream;
  }

  function CreateCrossRealmTransformReadable(port) {
    let backpressurePromise = v8.createPromise();
    let finished = false;

    callFunction(binding.EventTarget_addEventListener, port, 'message', evt => {
      const {type, value} = callFunction(binding.MessageEvent_data_get, evt);
      // assert(type === kChunk || type === kClose || type === kAbort ||
      //        type=kError);
      switch (type) {
        case kChunk:
          if (finished) {
            return;
          }
          binding.ReadableStreamDefaultControllerEnqueue(controller, value);
          resolvePromise(backpressurePromise);
          backpressurePromise = v8.createPromise();
          break;

        case kClose:
          if (finished) {
            return;
          }
          finished = true;
          binding.ReadableStreamDefaultControllerClose(controller);
          callFunction(binding.MessagePort_close, port);
          break;

        case kAbort:
        case kError:
          if (finished) {
            return;
          }
          finished = true;
          binding.ReadableStreamDefaultControllerError(
              controller, unpackReason(value));
          callFunction(binding.MessagePort_close, port);
          break;
      }
    });

    callFunction(
        binding.EventTarget_addEventListener, port, 'messageerror', () => {
          const error = new binding.DOMException('chunk could not be cloned',
                                                 'DataCloneError');
          callFunction(binding.MessagePort_postMessage, port,
                       {type: kError, value: packReason(error)});
          callFunction(binding.MessagePort_close, port);
          binding.ReadableStreamDefaultControllerError(controller, error);
        });

    callFunction(binding.MessagePort_start, port);

    const stream = binding.CreateReadableStream(
        () => undefined,
        () => {
          callFunction(
              binding.MessagePort_postMessage, port,
              {type: kPull, value: undefined});
          return backpressurePromise;
        },
        reason => {
          finished = true;
          callFunction(
              binding.MessagePort_postMessage, port,
              {type: kCancel, value: packReason(reason)});
          callFunction(binding.MessagePort_close, port);
          return Promise_resolve();
        },
        /* highWaterMark = */ 0);

    const controller = binding.getReadableStreamController(stream);
    return stream;
  }

  binding.streamOperations = {
    _queue,
    _queueTotalSize,
    hasOwnPropertyNoThrow,
    rejectPromise,
    resolvePromise,
    markPromiseAsHandled,
    promiseState,
    CreateAlgorithmFromUnderlyingMethod,
    CreateAlgorithmFromUnderlyingMethodPassingController,
    CreateCrossRealmTransformWritable,
    CreateCrossRealmTransformReadable,
    DequeueValue,
    EnqueueValueWithSize,
    PeekQueueValue,
    ResetQueue,
    ValidateAndNormalizeHighWaterMark,
    MakeSizeAlgorithmFromSizeFunction,
    CallOrNoop1,
    PromiseCall2
  };
});
