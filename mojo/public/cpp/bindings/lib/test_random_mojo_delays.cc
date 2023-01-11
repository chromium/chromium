// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/lib/binding_state.h"
#include "mojo/public/cpp/bindings/lib/test_random_mojo_delays.h"

namespace mojo {
namespace internal {

namespace {
constexpr int kInverseProbabilityOfDelay = 8;
constexpr int kInverseProbabilityOfNotResuming = 10;
constexpr base::TimeDelta kMillisecondsToResume = base::Milliseconds(2);
constexpr base::TimeDelta kPauseBindingsFrequency = base::Milliseconds(7);
}  // namespace

// TODO(mpdenton) This only adds random delays on method call processing. This
// also should add random delays on response processing. It is a mistake if a
// user assumes a response callback is received and run before a subsequent
// asynch call (over a different message pipe), and these random delays won't
// make it any more likely to find the mistake during testing.
class RandomMojoDelays {
 public:
  RandomMojoDelays()
      : runner_for_pauses_(base::ThreadPool::CreateSequencedTaskRunner({})) {
    DETACH_FROM_SEQUENCE(runner_for_pauses_sequence_checker);
  }

  void Start() {
    runner_for_pauses_->PostTask(
        FROM_HERE,
        base::BindOnce(&RandomMojoDelays::PauseRandomBindingStateBases,
                       base::Unretained(this)));
  }

  // TODO(mpdenton) what about bindings with associated interfaces? Apparently
  // you cannot pause on those? May need to change DCHECK to if(...) return;
  void AddBindingStateBase(scoped_refptr<base::SequencedTaskRunner> runner,
                           base::WeakPtr<BindingStateBase> binding_state_base) {
    runner_for_pauses_->PostTask(
        FROM_HERE,
        base::BindOnce(&RandomMojoDelays::AddBindingStateBaseInternal,
                       base::Unretained(this), std::move(runner),
                       std::move(binding_state_base)));
  }

 private:
  using BindingList = std::list<base::WeakPtr<BindingStateBase>>;

  // Must be called on |runner_for_pauses_| sequence. Adds a BindingStateBase
  // for random pausing purposes.
  void AddBindingStateBaseInternal(
      scoped_refptr<base::SequencedTaskRunner> runner,
      base::WeakPtr<BindingStateBase> binding_state_base) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(runner_for_pauses_sequence_checker);

    binding_state_base_map_[std::move(runner)].push_back(
        std::move(binding_state_base));
  }

  // Adds a list of BindingStateBases to be randomly paused. Used to re-attach
  // a list of BindingStateBases to the map after randomly pausing some of the
  // bindings on their bound sequences.
  void AddBindingStateBaseList(scoped_refptr<base::SequencedTaskRunner> runner,
                               BindingList binding_state_bases) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(runner_for_pauses_sequence_checker);

    BindingList& list = binding_state_base_map_[std::move(runner)];
    list.splice(list.end(), std::move(binding_state_bases));
  }

  // Resumes all bindings in |paused_binding_state_bases|.
  void ResumeFrozenBindingStateBasesOnTaskRunner(
      BindingList binding_state_bases,
      BindingList paused_binding_state_bases) {
    auto it = paused_binding_state_bases.begin();
    while (it != paused_binding_state_bases.end()) {
      base::WeakPtr<BindingStateBase> wptr = *it;
      if (!wptr) {
        // This WeakPtr was invalidated. We'll delete it
        // from the binding_state_bases list on the next PauseLoop.
        it = paused_binding_state_bases.erase(it);
        continue;
      }
      // Skip the resume with a 1/kInverseProbabilityOfNotResuming chance.
      if (base::RandInt(1, kInverseProbabilityOfNotResuming) >= 2) {
        wptr->ResumeIncomingMethodCallProcessing();
        it = paused_binding_state_bases.erase(it);
        continue;
      }
      it++;
    }
    if (!paused_binding_state_bases.empty()) {
      // Because we haven't resumed all the bindings, we should schedule another
      // resumption task in the future.
      // TODO(mpdenton) similar problem as below: can freeze shutdown if we
      // forget to unpause bindings.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &RandomMojoDelays::ResumeFrozenBindingStateBasesOnTaskRunner,
              base::Unretained(this), std::move(binding_state_bases),
              std::move(paused_binding_state_bases)),
          kMillisecondsToResume);
      return;
    }
    // Re-attach the bindings to the global map for future pausing.
    runner_for_pauses_->PostTask(
        FROM_HERE,
        base::BindOnce(&RandomMojoDelays::AddBindingStateBaseList,
                       base::Unretained(this),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       std::move(binding_state_bases)));
  }

  // Pause a random selection of bindings in the list |binding_state_bases|,
  // and set them to resume in the future.
  void PauseRandomBindingStateBasesOnTaskRunner(
      BindingList binding_state_bases) {
    BindingList paused_binding_state_bases;
    auto it = binding_state_bases.begin();
    while (it != binding_state_bases.end()) {
      // Remove any BindingStateBases that have been destroyed already.
      base::WeakPtr<BindingStateBase> wptr = *it;
      if (!wptr) {
        it = binding_state_bases.erase(it);
        continue;
      }
      if (base::RandInt(1, kInverseProbabilityOfDelay) >= 2) {
        it++;
        continue;
      }
      wptr->PauseIncomingMethodCallProcessing();
      paused_binding_state_bases.push_back(wptr);
      it++;
    }
    // Set the bindings to resume soon.
    // TODO(mpdenton) may cause deadlock on shutdown if this doesn't run. But
    // there is no PostDelayedTask for a SequencedTaskRunner.
    if (paused_binding_state_bases.size() > 0) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &RandomMojoDelays::ResumeFrozenBindingStateBasesOnTaskRunner,
              base::Unretained(this), std::move(binding_state_bases),
              std::move(paused_binding_state_bases)),
          kMillisecondsToResume);
    } else if (binding_state_bases.size() > 0) {
      // If we did not pause any bindings, re-attach the bindings to the global
      // map for future pausing, if there are any left after deleting all the
      // invalidated weak ptrs.
      runner_for_pauses_->PostTask(
          FROM_HERE,
          base::BindOnce(&RandomMojoDelays::AddBindingStateBaseList,
                         base::Unretained(this),
                         base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(binding_state_bases)));
    }
  }

  // Post tasks to every sequence with bound Mojo bindings, telling each to
  // pause random Mojo bindings bound on the respective sequence.
  void PauseRandomBindingStateBases() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(runner_for_pauses_sequence_checker);

    auto map_it = binding_state_base_map_.begin();
    while (map_it != binding_state_base_map_.end()) {
      // Tell sequence to randomly pause some of its bindings.
      map_it->first->PostTask(
          FROM_HERE,
          base::BindOnce(
              &RandomMojoDelays::PauseRandomBindingStateBasesOnTaskRunner,
              base::Unretained(this), std::move(map_it->second)));
      // Erase the current key-value pair (it will be re-added if necessary
      // after resuming the bindings--for now, drop the reference to the
      // SequencedTaskRunner).
      map_it = binding_state_base_map_.erase(map_it);
    }
    // Post delayed task, instead of using a RepeatingTimer, to avoid
    // overwhelming the task scheduling.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RandomMojoDelays::PauseRandomBindingStateBases,
                       base::Unretained(this)),
        kPauseBindingsFrequency);
  }

  scoped_refptr<base::SequencedTaskRunner> runner_for_pauses_;
  std::map<scoped_refptr<base::SequencedTaskRunner>, BindingList>
      binding_state_base_map_;
  SEQUENCE_CHECKER(runner_for_pauses_sequence_checker);
};

RandomMojoDelays& GetRandomMojoDelays() {
  static base::NoDestructor<RandomMojoDelays> random_mojo_delays;
  return *random_mojo_delays;
}

void MakeBindingRandomlyPaused(
    scoped_refptr<base::SequencedTaskRunner> runner,
    base::WeakPtr<BindingStateBase> binding_state_base) {
  GetRandomMojoDelays().AddBindingStateBase(std::move(runner),
                                            binding_state_base);
}

}  // namespace internal

void BeginRandomMojoDelays() {
  internal::GetRandomMojoDelays().Start();
}

}  // namespace mojo
