# Threading Naming Conventions

## Thread and Sequence Names

- `UIThread`: The browser UI thread (`BrowserThread::UI`).
- `PPSCoreSequence`: The non-UI `base::SequencedTaskRunner` on which
  `PrePrefetchServiceCore` runs.
- `NonUITriggerThread`: A non-UI thread (other than `PPSCoreSequence`) on which
  off-the-main-thread preloading operations (typically preprefetches) are
  triggered.
  Names like `StartPrePrefetch` that imply off-the-main-thread triggering
  don't need to additionally specify `NonUITriggerThread`.
- `NonUIThread`: Any thread or sequence other than the UI thread, not
  necessarily the specific threads/sequences above.

`OffTheMainThread` and `OMT` still refer to the feature concept, but shouldn't
be used as thread names.

## Function Names

Not every function name needs to encode threading behavior. When doing so, use
the forms below (`<Thread>` represents a thread/sequence name above):

- `DoSomethingOn<Thread>()`: Must be called on `<Thread>`.
- `PostDoSomethingTo<Thread>()`: Posts a task to `DoSomething` asynchronously
  on `<Thread>`, returning without blocking.
- `PostDoSomethingTo<Thread>AndWait()`: Posts a task to `DoSomething` on
  `<Thread>`, blocking the calling thread until completion.
  Must not be called on `<Thread>`.

Omit `On<Thread>` / `To<Thread>` when `<Thread>` is already clear from the
operation name or the owning class contract.

## Other Names

Follow the convention above, with applicable adaptation (e.g.
`non_ui_trigger_thread` for variable names). Namely:

- `something_on_<Thread>`: Must only be used on `<Thread>`. For pointers or
  handles (e.g. `std::unique_ptr<T>`, `base::WeakPtr<T>`), the target `T` must
  only be accessed or destroyed (and `base::WeakPtr<T>` validity checked) on
  `<Thread>`, while the pointer or handle itself may be stored/copied/moved etc.
  on any thread, as long as allowed by the pointer/handle class contracts.

Do NOT use `<Thread>_something` format, because it's quite ambiguous (e.g.
`something` on/to/from/of the thread?).
