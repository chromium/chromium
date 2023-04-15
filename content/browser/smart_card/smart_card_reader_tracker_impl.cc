// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_reader_tracker_impl.h"
#include "base/functional/callback.h"

namespace content {
namespace {

// Special string that, when specified as the reader name in GetStatusChange(),
// enables the client to be notified when a new smart card reader is added to
// the system.
// This a hack put on top of the PC/SC spec (which predates USB) and supported
// by most winscard implementations.
static const char kPnpNotification[] = R"(\\?PnP?\Notification)";

constexpr auto kStatusChangeTimeout = base::Minutes(5);

// Converts the mess that is the SCARD_STATE_* flags into a value of our flat
// enumeration.
//
// Shouldn't be called with `unaware`, `ignore` or `unknown` flags set as they
// have special meaning and should be handled elsewhere.
// Ie, they do not translate to a blink::mojom::SmartCardReaderState value.
blink::mojom::SmartCardReaderState ToBlinkSmartCardReaderState(
    const device::mojom::SmartCardReaderStateFlags& flags) {
  CHECK(!flags.unaware);
  CHECK(!flags.ignore);
  CHECK(!flags.unknown);

  if (flags.unavailable) {
    return blink::mojom::SmartCardReaderState::kUnavailable;
  }

  if (flags.empty) {
    return blink::mojom::SmartCardReaderState::kEmpty;
  }

  if (flags.exclusive) {
    if (!flags.present) {
      // The browser has no control of the platforms' PC/SC stack. Thus even
      // though this combination shouldn't happen, technically nothing stops the
      // PC/SC stack from returning such a combination of flags.
      // Same applies for other warnings in this function.
      LOG(WARNING) << "It's invalid to have SCARD_STATE_EXCLUSIVE without "
                      "SCARD_STATE_PRESENT.";
    }
    return blink::mojom::SmartCardReaderState::kExclusive;
  }

  if (flags.inuse) {
    if (!flags.present) {
      LOG(WARNING) << "It's invalid to have SCARD_STATE_INUSE without "
                      "SCARD_STATE_PRESENT.";
    }
    return blink::mojom::SmartCardReaderState::kInuse;
  }

  if (flags.mute) {
    if (!flags.present) {
      LOG(WARNING) << "It's invalid to have SCARD_STATE_MUTE without "
                      "SCARD_STATE_PRESENT.";
    }
    return blink::mojom::SmartCardReaderState::kMute;
  }

  if (flags.unpowered) {
    if (!flags.present) {
      LOG(WARNING) << "It's invalid to have SCARD_STATE_UNPOWERED without "
                      "SCARD_STATE_PRESENT.";
    }
    return blink::mojom::SmartCardReaderState::kUnpowered;
  }

  if (flags.present) {
    // There is a card and it's powered, responsive and not in use
    // by any other application.
    return blink::mojom::SmartCardReaderState::kPresent;
  }

  LOG(ERROR) << "Invalid ReaderStateFlags";
  return blink::mojom::SmartCardReaderState::kUnavailable;
}

void FailRequests(base::queue<SmartCardReaderTracker::StartCallback>& requests,
                  device::mojom::SmartCardError error) {
  while (!requests.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(requests.front()),
            blink::mojom::SmartCardGetReadersResult::NewError(error)));

    requests.pop();
  }
}

}  // namespace

// Contains the known state of a smart card reader.
class SmartCardReaderTrackerImpl::Reader {
 public:
  explicit Reader(const device::mojom::SmartCardReaderStateOut& state_out)
      : blink_reader_info_(blink::mojom::SmartCardReaderInfo::New(
            state_out.reader,
            ToBlinkSmartCardReaderState(*state_out.event_state.get()),
            state_out.answer_to_reset)) {
    CopyStateFlags(*state_out.event_state.get());
  }

  // Returns whether any change was made.
  bool Update(const device::mojom::SmartCardReaderStateOut& state_out) {
    CHECK_EQ(state_out.reader, blink_reader_info_->name);
    bool changed = false;

    if (blink_reader_info_->atr != state_out.answer_to_reset) {
      blink_reader_info_->atr = state_out.answer_to_reset;
      changed = true;
    }

    blink::mojom::SmartCardReaderState new_blink_state =
        ToBlinkSmartCardReaderState(*state_out.event_state.get());
    if (new_blink_state != blink_reader_info_->state) {
      blink_reader_info_->state = new_blink_state;
      changed = true;
    }

    CopyStateFlags(*state_out.event_state.get());

    return changed;
  }

  device::mojom::SmartCardReaderStateFlagsPtr CreateCurrentStateFlags() {
    auto flags = device::mojom::SmartCardReaderStateFlags::New();

    flags->unavailable = unavailable_;
    flags->empty = empty_;
    flags->present = present_;
    flags->exclusive = exclusive_;
    flags->inuse = inuse_;
    flags->mute = mute_;
    flags->unpowered = unpowered_;

    return flags;
  }

  const blink::mojom::SmartCardReaderInfo& blink_reader_info() const {
    return *blink_reader_info_.get();
  }

 private:
  void CopyStateFlags(
      const device::mojom::SmartCardReaderStateFlags& state_flags) {
    unavailable_ = state_flags.unavailable;
    empty_ = state_flags.empty;
    present_ = state_flags.present;
    exclusive_ = state_flags.exclusive;
    inuse_ = state_flags.inuse;
    mute_ = state_flags.mute;
    unpowered_ = state_flags.unpowered;
  }

  blink::mojom::SmartCardReaderInfoPtr blink_reader_info_;

  // As received from the last GetStatusChange() run.
  // To be used on the next GetStatusChange() call as the known/expected state.
  //
  // Note that there's no strict 1:1 mapping between those and
  // blink::mojom::SmartCardReaderState. Therefore we have to store them
  // separately.
  bool unavailable_;
  bool empty_;
  bool present_;
  bool exclusive_;
  bool inuse_;
  bool mute_;
  bool unpowered_;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::State

// Represents a state in the `SmartCardReaderTracker` state machine.
// It defines how the `SmartCardReaderTracker` reacts to calls to its public
// methods and to its callbacks.
class SmartCardReaderTrackerImpl::State {
 public:
  explicit State(SmartCardReaderTrackerImpl& tracker) : tracker_(tracker) {}
  virtual ~State() = default;
  virtual std::string ToString() const = 0;
  virtual void Enter() {}
  virtual void Start(StartCallback) {}

 protected:
  // Owns `this`.
  const raw_ref<SmartCardReaderTrackerImpl> tracker_;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Uninitialized declaration

// Initial state of `SmartCardReaderTracker`.
class SmartCardReaderTrackerImpl::Uninitialized
    : public SmartCardReaderTrackerImpl::State {
 public:
  explicit Uninitialized(SmartCardReaderTrackerImpl& tracker)
      : State(tracker) {}
  std::string ToString() const override;
  void Start(StartCallback) override;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitReadersList declaration

// `SmartCardReaderTracker` called `ListReaders` and is waiting for its result.
class SmartCardReaderTrackerImpl::WaitReadersList
    : public SmartCardReaderTrackerImpl::State {
 public:
  WaitReadersList(SmartCardReaderTrackerImpl& tracker,
                  mojo::Remote<device::mojom::SmartCardContext> context,
                  base::queue<StartCallback> pending_get_readers_requests =
                      base::queue<StartCallback>());
  WaitReadersList(
      SmartCardReaderTrackerImpl& tracker,
      mojo::PendingRemote<device::mojom::SmartCardContext> pending_context,
      base::queue<StartCallback> pending_get_readers_requests);
  ~WaitReadersList() override;

  std::string ToString() const override;
  void Enter() override;
  void Start(StartCallback callback) override;

 private:
  void OnListReadersDone(device::mojom::SmartCardListReadersResultPtr result);
  void RemoveAbsentReaders(const std::vector<std::string>& current_readers);
  std::vector<std::string> IdentifyNewReaders(
      const std::vector<std::string>& current_readers);
  void ReplyNoReaders();

  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::queue<StartCallback> pending_get_readers_requests_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitReadersList>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::KeepContext

// Keeps a valid `SmartCardContext` until the next
// `SmartCardReaderTrackerImpl::Start` call.
// Goes to `Uninitialized` state on timeout.
class SmartCardReaderTrackerImpl::KeepContext
    : public SmartCardReaderTrackerImpl::State {
 public:
  KeepContext(SmartCardReaderTrackerImpl& tracker,
              mojo::Remote<device::mojom::SmartCardContext> context)
      : State(tracker), context_(std::move(context)) {}
  ~KeepContext() override = default;
  std::string ToString() const override { return "KeepContext"; }

  void Enter() override {
    // TODO(crbug.com/1386175): start timer and go to Uninitialized on timeout.
  }

  void Start(StartCallback callback) override {
    pending_get_readers_requests_.push(std::move(callback));
    tracker_->ChangeState(std::make_unique<WaitReadersList>(
        *tracker_, std::move(context_),
        std::move(pending_get_readers_requests_)));
  }

 private:
  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::queue<StartCallback> pending_get_readers_requests_;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Tracking

// Main state of `SmartCardReaderTracker`.
//
// `SmartCardReaderTrackerImpl::readers_` has an up to date state of all
// available smart card readers.
//
// It has a pending `SmartCardContext::GetStatusChange`request which returns
// once a change takes place.
class SmartCardReaderTrackerImpl::Tracking
    : public SmartCardReaderTrackerImpl::State {
 public:
  Tracking(SmartCardReaderTrackerImpl& tracker,
           mojo::Remote<device::mojom::SmartCardContext> context)
      : State(tracker), context_(std::move(context)) {}

  std::string ToString() const override { return "Tracking"; }

  void Enter() override {
    CHECK(tracker_->CanTrack());

    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;

    // Get notified when a reader is added to the system.
    if (tracker_->context_supports_reader_added_) {
      auto state = device::mojom::SmartCardReaderStateIn::New(
          kPnpNotification, device::mojom::SmartCardReaderStateFlags::New());
      reader_states.push_back(std::move(state));
    }

    // Get notified when a known, existing, reader changes its state or is
    // removed from the system.
    for (auto const& [name, reader] : tracker_->readers_) {
      reader_states.push_back(device::mojom::SmartCardReaderStateIn::New(
          name, reader->CreateCurrentStateFlags()));
    }

    // Instead of waiting indefinitely until anything changes, wait
    // for some time and then expect to receive a timeout from PC/SC.
    // Useful as a way of telling whether the PC/SC backend is still "alive".
    context_->GetStatusChange(
        kStatusChangeTimeout, std::move(reader_states),
        base::BindOnce(
            &SmartCardReaderTrackerImpl::Tracking::OnGetStatusChangeDone,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void Start(StartCallback callback) override {
    // TODO(crbug.com/1386175): Cancel current GetStatusChange to force a
    // refresh if we are in this state for longer than MIN_REFRESH_INTERVAL.
    // otherwise return cached result as below.
    tracker_->GetReadersFromCache(std::move(callback));
  }

 private:
  void OnGetStatusChangeDone(
      device::mojom::SmartCardStatusChangeResultPtr result) {
    if (result->is_error()) {
      const device::mojom::SmartCardError error = result->get_error();
      if (error == device::mojom::SmartCardError::kCancelled ||
          error == device::mojom::SmartCardError::kTimeout) {
        TrackChangesOrGiveUp();
      } else {
        tracker_->observer_list_.NotifyError(result->get_error());
        tracker_->ChangeState(std::make_unique<Uninitialized>(*tracker_));
      }
      return;
    }

    tracker_->UpdateCache(result->get_reader_states());
    TrackChangesOrGiveUp();
  }

  void TrackChangesOrGiveUp() {
    if (tracker_->CanTrack()) {
      tracker_->ChangeState(
          std::make_unique<WaitReadersList>(*tracker_, std::move(context_)));
    } else {
      tracker_->ChangeState(
          std::make_unique<KeepContext>(*tracker_, std::move(context_)));
    }
  }

  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::Tracking> weak_ptr_factory_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitInitialReaderStatus

// It's waiting for a `SmartCardContext::GetStatusChange` request to return
// with information on the available, but unknown to the tracker, smart card
// readers.
class SmartCardReaderTrackerImpl::WaitInitialReaderStatus
    : public SmartCardReaderTrackerImpl::State {
 public:
  explicit WaitInitialReaderStatus(
      SmartCardReaderTrackerImpl& tracker,
      mojo::Remote<device::mojom::SmartCardContext> context,
      base::queue<StartCallback> pending_get_readers_requests,
      std::vector<std::string>& new_readers)
      : State(tracker),
        context_(std::move(context)),
        pending_get_readers_requests_(std::move(pending_get_readers_requests)),
        new_readers_(new_readers) {}
  std::string ToString() const override { return "WaitInitialReaderStatus"; }
  void Enter() override {
    CHECK(!new_readers_.empty());

    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;

    for (const std::string& reader_name : new_readers_) {
      CHECK_EQ(tracker_->readers_.count(reader_name), size_t(0));

      auto state = device::mojom::SmartCardReaderStateIn::New();
      state->reader = reader_name;

      state->current_state = device::mojom::SmartCardReaderStateFlags::New(
          /*unaware=*/true, false, false, false, false, false, false, false,
          false, false, false);

      reader_states.push_back(std::move(state));
    }

    context_->GetStatusChange(
        base::TimeDelta::Max(), std::move(reader_states),
        base::BindOnce(&SmartCardReaderTrackerImpl::WaitInitialReaderStatus::
                           OnGetStatusChangeDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Start(StartCallback callback) override {
    pending_get_readers_requests_.push(std::move(callback));
  }

 private:
  void OnGetStatusChangeDone(
      device::mojom::SmartCardStatusChangeResultPtr result) {
    if (result->is_error()) {
      FailRequests(pending_get_readers_requests_,
                   device::mojom::SmartCardError::kNoService);
      tracker_->observer_list_.NotifyError(result->get_error());
      tracker_->ChangeState(std::make_unique<Uninitialized>(*tracker_));
      return;
    }

    tracker_->UpdateCache(result->get_reader_states());

    while (!pending_get_readers_requests_.empty()) {
      tracker_->GetReadersFromCache(
          std::move(pending_get_readers_requests_.front()));
      pending_get_readers_requests_.pop();
    }

    if (tracker_->CanTrack()) {
      tracker_->ChangeState(
          std::make_unique<Tracking>(*tracker_, std::move(context_)));
    } else {
      tracker_->ChangeState(
          std::make_unique<KeepContext>(*tracker_, std::move(context_)));
    }
  }

  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::queue<StartCallback> pending_get_readers_requests_;
  std::vector<std::string> new_readers_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitInitialReaderStatus>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitReadersList implementation

SmartCardReaderTrackerImpl::WaitReadersList::WaitReadersList(
    SmartCardReaderTrackerImpl& tracker,
    mojo::Remote<device::mojom::SmartCardContext> context,
    base::queue<StartCallback> pending_get_readers_requests)
    : State(tracker),
      context_(std::move(context)),
      pending_get_readers_requests_(std::move(pending_get_readers_requests)) {}

SmartCardReaderTrackerImpl::WaitReadersList::WaitReadersList(
    SmartCardReaderTrackerImpl& tracker,
    mojo::PendingRemote<device::mojom::SmartCardContext> pending_context,
    base::queue<StartCallback> pending_get_readers_requests)
    : State(tracker),
      context_(std::move(pending_context)),
      pending_get_readers_requests_(std::move(pending_get_readers_requests)) {}

SmartCardReaderTrackerImpl::WaitReadersList::~WaitReadersList() = default;

std::string SmartCardReaderTrackerImpl::WaitReadersList::ToString() const {
  return "WaitReadersList";
}

void SmartCardReaderTrackerImpl::WaitReadersList::Enter() {
  context_->ListReaders(base::BindOnce(
      &SmartCardReaderTrackerImpl::WaitReadersList::OnListReadersDone,
      weak_ptr_factory_.GetWeakPtr()));
}

void SmartCardReaderTrackerImpl::WaitReadersList::Start(
    StartCallback callback) {
  pending_get_readers_requests_.push(std::move(callback));
}

void SmartCardReaderTrackerImpl::WaitReadersList::OnListReadersDone(
    device::mojom::SmartCardListReadersResultPtr result) {
  std::vector<std::string> current_readers;

  if (result->is_error()) {
    if (result->get_error() !=
        device::mojom::SmartCardError::kNoReadersAvailable) {
      FailRequests(pending_get_readers_requests_, result->get_error());
      tracker_->ChangeState(std::make_unique<Uninitialized>(*tracker_));
      return;
    }
  } else {
    current_readers = result->get_readers();
  }

  RemoveAbsentReaders(current_readers);

  if (current_readers.empty()) {
    ReplyNoReaders();
    tracker_->ChangeState(
        std::make_unique<Tracking>(*tracker_, std::move(context_)));
    return;
  }

  std::vector<std::string> new_readers = IdentifyNewReaders(current_readers);

  if (new_readers.empty()) {
    // We already know about all existing readers and their states.
    // The cache can be considered still up to date.
    while (!pending_get_readers_requests_.empty()) {
      tracker_->GetReadersFromCache(
          std::move(pending_get_readers_requests_.front()));
      pending_get_readers_requests_.pop();
    }
    // And we can go straight to Tracking (skipping WaitInitialReaderStatus).
    tracker_->ChangeState(
        std::make_unique<Tracking>(*tracker_, std::move(context_)));
    return;
  }

  tracker_->ChangeState(std::make_unique<WaitInitialReaderStatus>(
      *tracker_, std::move(context_), std::move(pending_get_readers_requests_),
      new_readers));
}

void SmartCardReaderTrackerImpl::WaitReadersList::RemoveAbsentReaders(
    const std::vector<std::string>& current_readers) {
  std::unordered_set<std::string> current_readers_set;
  for (const auto& reader_name : current_readers) {
    current_readers_set.insert(reader_name);
  }

  for (auto it = tracker_->readers_.begin(); it != tracker_->readers_.end();) {
    std::string reader_name = it->first;
    if (current_readers_set.count(reader_name) == 0) {
      std::unique_ptr<Reader> reader = std::move(it->second);
      it = tracker_->readers_.erase(it);
      tracker_->observer_list_.NotifyReaderRemoved(reader->blink_reader_info());
    } else {
      ++it;
    }
  }
}

std::vector<std::string>
SmartCardReaderTrackerImpl::WaitReadersList::IdentifyNewReaders(
    const std::vector<std::string>& current_readers) {
  std::vector<std::string> new_readers;
  for (const std::string& reader_name : current_readers) {
    if (tracker_->readers_.count(reader_name) == 0) {
      new_readers.push_back(reader_name);
    }
  }
  return new_readers;
}

void SmartCardReaderTrackerImpl::WaitReadersList::ReplyNoReaders() {
  while (!pending_get_readers_requests_.empty()) {
    std::move(pending_get_readers_requests_.front())
        .Run(blink::mojom::SmartCardGetReadersResult::NewReaders(
            std::vector<::blink::mojom::SmartCardReaderInfoPtr>()));
    pending_get_readers_requests_.pop();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitContext

// State where the `SmartCardReaderTracker` is waiting for a
// `SmartCardContextFactory::CreateContext` call to return.
class SmartCardReaderTrackerImpl::WaitContext
    : public SmartCardReaderTrackerImpl::State {
 public:
  std::string ToString() const override { return "WaitContext"; }
  WaitContext(SmartCardReaderTrackerImpl& tracker, StartCallback callback)
      : State(tracker) {
    pending_get_readers_requests_.push(std::move(callback));
  }

  void Enter() override {
    tracker_->context_factory_->CreateContext(base::BindOnce(
        &SmartCardReaderTrackerImpl::WaitContext::OnEstablishContextDone,
        weak_ptr_factory_.GetWeakPtr()));
  }

  void Start(StartCallback callback) override {
    pending_get_readers_requests_.push(std::move(callback));
  }

 private:
  void OnEstablishContextDone(
      device::mojom::SmartCardCreateContextResultPtr result) {
    if (result->is_error()) {
      FailRequests(pending_get_readers_requests_, result->get_error());
      tracker_->ChangeState(std::make_unique<Uninitialized>(*tracker_));
      return;
    }

    tracker_->ChangeState(std::make_unique<WaitReadersList>(
        *tracker_, std::move(result->get_context()),
        std::move(pending_get_readers_requests_)));
  }

  base::queue<StartCallback> pending_get_readers_requests_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitContext>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Uninitialized implementation

std::string SmartCardReaderTrackerImpl::Uninitialized::ToString() const {
  return "Uninitialized";
}

void SmartCardReaderTrackerImpl::Uninitialized::Start(StartCallback callback) {
  tracker_->ChangeState(
      std::make_unique<WaitContext>(*tracker_, std::move(callback)));
}

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl

SmartCardReaderTrackerImpl::SmartCardReaderTrackerImpl(
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory,
    bool context_supports_reader_added)
    : state_(std::make_unique<Uninitialized>(*this)),
      context_factory_(std::move(context_factory)),
      context_supports_reader_added_(context_supports_reader_added) {}

SmartCardReaderTrackerImpl::~SmartCardReaderTrackerImpl() = default;

void SmartCardReaderTrackerImpl::Start(Observer* observer,
                                       StartCallback callback) {
  observer_list_.AddObserverIfMissing(observer);
  state_->Start(std::move(callback));
}

void SmartCardReaderTrackerImpl::Stop(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  // TODO(crbug.com/1386175): call state if there are no readers left. Tracking
  // state should cancel its pending GetStatusChange.
}

void SmartCardReaderTrackerImpl::ChangeState(
    std::unique_ptr<State> next_state) {
  CHECK(next_state);

  auto current_state = std::move(state_);

  VLOG(1) << "ChangeState: " << current_state->ToString() << " -> "
          << next_state->ToString();
  state_ = std::move(next_state);
  state_->Enter();

  // This method is invoked from inside `current_state`, so we postpone
  // destruction to ensure it has a chance to finish its current method
  // without crashing because it was deleted.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(current_state));
}

bool SmartCardReaderTrackerImpl::CanTrack() const {
  return !observer_list_.empty() &&
         (!readers_.empty() || context_supports_reader_added_);
}

void SmartCardReaderTrackerImpl::AddReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  auto unique_reader = std::make_unique<Reader>(state_out);

  const blink::mojom::SmartCardReaderInfo& reader_info =
      unique_reader->blink_reader_info();

  readers_[state_out.reader] = std::move(unique_reader);

  if (!context_supports_reader_added_) {
    // For consistency, never send it if not supported.
    // Otherwise one user calling Start() would have a side
    // effect on the obsevers of other users as they would
    // be notified even though the didn't call Start()
    // themselves.
    return;
  }

  observer_list_.NotifyReaderAdded(reader_info);
}

void SmartCardReaderTrackerImpl::AddOrUpdateReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  auto it = readers_.find(state_out.reader);
  if (it == readers_.end()) {
    AddReader(state_out);
  } else {
    std::unique_ptr<Reader>& reader = it->second;
    if (reader->Update(state_out)) {
      observer_list_.NotifyReaderChanged(reader->blink_reader_info());
    }
  }
}

void SmartCardReaderTrackerImpl::RemoveReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  auto it = readers_.find(state_out.reader);
  if (it == readers_.end()) {
    return;
  }

  std::unique_ptr<Reader> reader = std::move(it->second);
  readers_.erase(it);

  observer_list_.NotifyReaderRemoved(reader->blink_reader_info());
}

void SmartCardReaderTrackerImpl::GetReadersFromCache(StartCallback callback) {
  std::vector<::blink::mojom::SmartCardReaderInfoPtr> reader_list;
  reader_list.reserve(readers_.size());

  for (const auto& [_, reader] : readers_) {
    reader_list.push_back(reader->blink_reader_info().Clone());
  }

  std::move(callback).Run(blink::mojom::SmartCardGetReadersResult::NewReaders(
      std::move(reader_list)));
}

void SmartCardReaderTrackerImpl::UpdateCache(
    const std::vector<device::mojom::SmartCardReaderStateOutPtr>&
        reader_states) {
  for (const auto& reader_state : reader_states) {
    if (reader_state->reader == kPnpNotification) {
      // This is not an actual reader device.
      continue;
    }
    if (reader_state->event_state->unknown ||
        reader_state->event_state->ignore ||
        reader_state->event_state->unaware) {
      RemoveReader(*reader_state.get());
    } else {
      AddOrUpdateReader(*reader_state.get());
    }
  }
}

}  // namespace content
