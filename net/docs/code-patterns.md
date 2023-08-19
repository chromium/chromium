# Chrome Network Stack Common Coding Patterns

## Combined error and byte count into a single value

At many places in the network stack, functions return a value that, if
positive, indicate a count of bytes that the the function read or
wrote, and if negative, indicates a network stack error code (see
[net_error_list.h][]).
Zero indicates either `net::OK` or zero bytes read (usually EOF)
depending on the context. This pattern is generally specified by
an `int` return type.

Many functions also have variables (often named `result` or `rv`) containing
such a value; this is especially common in the [DoLoop](#DoLoop) pattern
described below.

## Sync/Async Return

Many network stack routines may return synchronously or
asynchronously. These functions generally return an int as described
above. There are three cases:

* If the value is positive or zero, that indicates a synchronous
  successful return, with a zero return value indicating either zero
  bytes/EOF or indicating `net::OK`, depending on context. If there
  is a callback argument, it is not invoked.
* If the value is negative and != `net::ERR_IO_PENDING`, it is an error
  code specifying a synchronous failure. If there is a callback argument,
  it is not invoked.
* If the return value is the special value `net::ERR_IO_PENDING`, it
  indicates that the routine will complete asynchronously. A reference to
  any provided IOBuffer will be retained by the called entity until
  completion, to be written into or read from as required. 
  If there is a callback argument, that callback will be called upon
  completion with the return value; if there is no callback argument, it
  usually means that some known callback mechanism will be employed.

## DoLoop

The DoLoop pattern is used in the network stack to construct simple
state machines. It is used for cases in which processing is basically
single-threaded and could be written in a single function, if that
function could block waiting for input. Generally, initiation of a
state machine is triggered by some method invocation by a class
consumer, and that state machine is driven (possibly across
asynchronous IO initiated by the class) until the operation requested
by the method invocation completes, at which point the state variable is
set to `STATE_NONE` and the consumer notified.  

Cases which do not fit into this single-threaded, single consumer
operation model are generally adapted in some way to fit the model,
either by multiple state machines (e.g. independent state machines for
reading and writing, if each can be initiated while the other is
outstanding) or by storing information across consumer invocations and
returns that can be used to restart the state machine in the proper
state. 

Any class using this pattern will contain an enum listing all states
of that machine, and define a function, `DoLoop()`, to drive that state
machine. If a class has multiple state machines (as above) it will
have multiple methods (e.g. `DoReadLoop()` and `DoWriteLoop()`) to drive
those different machines.

The characteristics of the DoLoop pattern are:

*   Each state has a corresponding function which is called by `DoLoop()`
    for handling when the state machine is in that state. Generally the
    states are named STATE`_<`STATE_NAME`>` (upper case separated by
    underscores), and the routine is named Do`<`StateName`>` (CamelCase).
    For example:

         enum State {
             STATE_NONE, 
             STATE_INIT,
             STATE_FOO,
             STATE_FOO_COMPLETE,
         };
         int DoInit();
         int DoFoo();
         int DoFooComplete(int result);

*   Each state handling function has two basic responsibilities in
    addition to state specific handling: Setting the data member
    (named `next_state_` or something similar)
    to specify the next state, and returning a `net::Error` (or combined
    error and byte count, as above). 
    
*   On each `DoLoop()` iteration, the function saves the next state to a local
    variable and resets to a default state (`STATE_NONE`),
    and then calls the appropriate state handling based on the
    original value of the next state. This looks like:

           do {
             State state = io_state_;
             next_state_ = STATE_NONE;
             switch (state) {
               case STATE_INIT:
                 result = DoInit();
                 break;
               ...

    This pattern is followed primarily to ensure that in the event of
    a bug where the next state isn't set, the loop terminates rather
    than loops infinitely. It's not a perfect mitigation, but works
    well as a defensive measure.
    
*   If a given state may complete asynchronously (for example,
    writing to an underlying transport socket), then there will often
    be split states, such as `STATE_WRITE` and
    `STATE_WRITE_COMPLETE`. The first state is responsible for
    starting/continuing the original operation, while the second state
    is responsible for handling completion (e.g. success vs error,
    complete vs. incomplete writes), and determining the next state to
    transition to. 
    
*   While the return value from each call is propagated through the loop
    to the next state, it is expected that for most state transitions the
    return value will be `net::OK`, and that an error return will also
    set the state to `STATE_NONE` or fail to override the default
    assignment to `STATE_DONE` to exit the loop and return that 
    error to the caller. This is often asserted with a DCHECK, e.g.

            case STATE_FOO:
                DCHECK_EQ(result, OK);
                result = DoFoo();
                break;

    The exception to this pattern is split states, where an IO
    operation has been dispatched, and the second state is handling
    the result. In that case, the second state's function takes the
    result code:
    
            case STATE_FOO_COMPLETE:
                result = DoFooComplete(result);
                break;
    
*   If the return value from the state handling function is
    `net::ERR_IO_PENDING`, that indicates that the function has arranged
    for `DoLoop()` to be called at some point in the future, when further
    progress can be made on the state transitions. The `next_state_` variable
    will have been set to the proper value for handling that incoming
    call. In this case, `DoLoop()` will exit. This often occurs between
    split states, as described above. 
    
*   The DoLoop mechanism is generally invoked in response to a consumer
    calling one of its methods. While the operation that method
    requested is occuring, the state machine stays active, possibly
    over multiple asynchronous operations and state transitions. When
    that operation is complete, the state machine transitions to
    `STATE_NONE` (by a `DoLoop()` callee not setting `next_state_`) or
    explicitly to `STATE_DONE` (indicating that the operation is
    complete *and* the state machine is not amenable to further
    driving). At this point the consumer is notified of the completion
    of the operation (by synchronous return or asynchronous callback).
 
    Note that this implies that when `DoLoop()` returns, one of two
    things will be true:
 
    * The return value will be `net::ERR_IO_PENDING`, indicating that the
      caller should take no action and instead wait for asynchronous
      notification. 
    * The state of the machine will be either `STATE_DONE` or `STATE_NONE`,
      indicating that the operation that first initiated the `DoLoop()` has
      completed. 
 
    This invariant reflects and enforces the single-threaded (though
    possibly asynchronous) nature of the driven state machine--the
    machine is always executing one requested operation.
    
*   `DoLoop()` is called from two places: a) methods exposed to the consumer
    for specific operations (e.g. `ReadHeaders()`), and b) an IO completion
    callbacks called asynchronously by spawned IO operations.

    In the first case, the return value from `DoLoop()` is returned directly
    to the caller; if the operation completed synchronously, that will
    contain the operation result, and if it completed asynchronously, it
    will be `net::ERR_IO_PENDING`. For example (from 
    `HttpStreamParser`, abridged for clarity): 

             int HttpStreamParser::ReadResponseHeaders(
                 CompletionOnceCallback callback) {
               DCHECK(io_state_ == STATE_NONE || io_state_ == STATE_DONE);
               DCHECK(callback_.is_null());
               DCHECK(!callback.is_null());

               int result = OK;
               io_state_ = STATE_READ_HEADERS;

               result = DoLoop(result);

               if (result == ERR_IO_PENDING)
                 callback_ = std::move(callback);

               return result > 0 ? OK : result;
             }

    In the second case, the IO completion callback will examine the
    return value from `DoLoop()`. If it is `net::ERR_IO_PENDING`, no
    further action will be taken, and the IO completion callback will be
    called again at some future point. If it is not
    `net::ERR_IO_PENDING`, that is a signal that the operation has
    completed, and the IO completion callback will call the appropriate
    consumer callback to notify the consumer that the operation has
    completed. Note that it is important that this callback be done
    from the IO completion callback and not from `DoLoop()` or a
    `DoLoop()` callee, both to support the sync/async error return
    (DoLoop and its callees don't know the difference) and to avoid
    consumer callbacks deleting the object out from under `DoLoop()`.
    Example: 

             void HttpStreamParser::OnIOComplete(int result) {
               result = DoLoop(result);

               if (result != ERR_IO_PENDING && !callback_.is_null())
                 std::move(callback_).Run(result);
             }
    
*   The DoLoop pattern has no concept of different events arriving for
    a single state; each state, if waiting, is waiting for one
    particular event, and when `DoLoop()` is invoked when the machine is
    in that state, it will handle that event. This reflects the
    single-threaded model for operations spawned by the state machine.

Public class methods generally have very little processing, primarily wrapping 
`DoLoop()`. For `DoLoop()` entry this involves setting the `next_state_`
variable, and possibly making copies of arguments into class members. For
`DoLoop()` exit, it involves inspecting the return and passing it back to
the caller, and in the asynchronous case, saving any passed completion callback
for executing by a future subsidiary IO completion (see above example). 

This idiom allows synchronous and asynchronous logic to be written in
the same fashion; it's all just state transition handling. For mostly
linear state diagrams, the handling code can be very easy to
comprehend, as such code is usually written linearly (in different
handling functions) in the order it's executed. 

For examples of this idiom, see

* [HttpStreamParser::DoLoop](https://source.chromium.org/chromium/chromium/src/+/HEAD:net/http/http_stream_parser.cc).
* [HttpNetworkTransaction::DoLoop](https://source.chromium.org/chromium/chromium/src/+/HEAD:net/http/http_network_transaction.cc)

[net_error_list.h]: https://chromium.googlesource.com/chromium/src/+/main/net/base/net_error_list.h#1
