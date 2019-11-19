//
// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/flags/internal/registry.h"

#include "absl/base/dynamic_annotations.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/flags/config.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

// --------------------------------------------------------------------
// FlagRegistry implementation
//    A FlagRegistry holds all flag objects indexed
//    by their names so that if you know a flag's name you can access or
//    set it.

namespace absl {
namespace flags_internal {
namespace {

void DestroyFlag(CommandLineFlag* flag) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  flag->Destroy();

  // CommandLineFlag handle object is heap allocated for non Abseil Flags.
  if (!flag->IsAbseilFlag()) {
    delete flag;
  }
}

}  // namespace

// --------------------------------------------------------------------
// FlagRegistry
//    A FlagRegistry singleton object holds all flag objects indexed
//    by their names so that if you know a flag's name (as a C
//    string), you can access or set it.  If the function is named
//    FooLocked(), you must own the registry lock before calling
//    the function; otherwise, you should *not* hold the lock, and
//    the function will acquire it itself if needed.
// --------------------------------------------------------------------

class FlagRegistry {
 public:
  FlagRegistry() = default;
  ~FlagRegistry() {
    for (auto& p : flags_) {
      DestroyFlag(p.second);
    }
  }

  // Store a flag in this registry.  Takes ownership of *flag.
  void RegisterFlag(CommandLineFlag* flag);

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION(lock_) { lock_.Lock(); }
  void Unlock() ABSL_UNLOCK_FUNCTION(lock_) { lock_.Unlock(); }

  // Returns the flag object for the specified name, or nullptr if not found.
  // Will emit a warning if a 'retired' flag is specified.
  CommandLineFlag* FindFlagLocked(absl::string_view name);

  // Returns the retired flag object for the specified name, or nullptr if not
  // found or not retired.  Does not emit a warning.
  CommandLineFlag* FindRetiredFlagLocked(absl::string_view name);

  static FlagRegistry* GlobalRegistry();  // returns a singleton registry

 private:
  friend class FlagSaverImpl;  // reads all the flags in order to copy them
  friend void ForEachFlagUnlocked(
      std::function<void(CommandLineFlag*)> visitor);

  // The map from name to flag, for FindFlagLocked().
  using FlagMap = std::map<absl::string_view, CommandLineFlag*>;
  using FlagIterator = FlagMap::iterator;
  using FlagConstIterator = FlagMap::const_iterator;
  FlagMap flags_;

  absl::Mutex lock_;

  // Disallow
  FlagRegistry(const FlagRegistry&);
  FlagRegistry& operator=(const FlagRegistry&);
};

FlagRegistry* FlagRegistry::GlobalRegistry() {
  static FlagRegistry* global_registry = new FlagRegistry;
  return global_registry;
}

namespace {

class FlagRegistryLock {
 public:
  explicit FlagRegistryLock(FlagRegistry* fr) : fr_(fr) { fr_->Lock(); }
  ~FlagRegistryLock() { fr_->Unlock(); }

 private:
  FlagRegistry* const fr_;
};

}  // namespace

void FlagRegistry::RegisterFlag(CommandLineFlag* flag) {
  FlagRegistryLock registry_lock(this);
  std::pair<FlagIterator, bool> ins =
      flags_.insert(FlagMap::value_type(flag->Name(), flag));
  if (ins.second == false) {  // means the name was already in the map
    CommandLineFlag* old_flag = ins.first->second;
    if (flag->IsRetired() != old_flag->IsRetired()) {
      // All registrations must agree on the 'retired' flag.
      flags_internal::ReportUsageError(
          absl::StrCat(
              "Retired flag '", flag->Name(),
              "' was defined normally in file '",
              (flag->IsRetired() ? old_flag->Filename() : flag->Filename()),
              "'."),
          true);
    } else if (flag->op != old_flag->op) {
      flags_internal::ReportUsageError(
          absl::StrCat("Flag '", flag->Name(),
                       "' was defined more than once but with "
                       "differing types. Defined in files '",
                       old_flag->Filename(), "' and '", flag->Filename(),
                       "' with types '", old_flag->Typename(), "' and '",
                       flag->Typename(), "', respectively."),
          true);
    } else if (old_flag->IsRetired()) {
      // Retired definitions are idempotent. Just keep the old one.
      DestroyFlag(flag);
      return;
    } else if (old_flag->Filename() != flag->Filename()) {
      flags_internal::ReportUsageError(
          absl::StrCat("Flag '", flag->Name(),
                       "' was defined more than once (in files '",
                       old_flag->Filename(), "' and '", flag->Filename(),
                       "')."),
          true);
    } else {
      flags_internal::ReportUsageError(
          absl::StrCat(
              "Something wrong with flag '", flag->Name(), "' in file '",
              flag->Filename(), "'. One possibility: file '", flag->Filename(),
              "' is being linked both statically and dynamically into this "
              "executable. e.g. some files listed as srcs to a test and also "
              "listed as srcs of some shared lib deps of the same test."),
          true);
    }
    // All cases above are fatal, except for the retired flags.
    std::exit(1);
  }
}

CommandLineFlag* FlagRegistry::FindFlagLocked(absl::string_view name) {
  FlagConstIterator i = flags_.find(name);
  if (i == flags_.end()) {
    return nullptr;
  }

  if (i->second->IsRetired()) {
    flags_internal::ReportUsageError(
        absl::StrCat("Accessing retired flag '", name, "'"), false);
  }

  return i->second;
}

CommandLineFlag* FlagRegistry::FindRetiredFlagLocked(absl::string_view name) {
  FlagConstIterator i = flags_.find(name);
  if (i == flags_.end() || !i->second->IsRetired()) {
    return nullptr;
  }

  return i->second;
}

// --------------------------------------------------------------------
// FlagSaver
// FlagSaverImpl
//    This class stores the states of all flags at construct time,
//    and restores all flags to that state at destruct time.
//    Its major implementation challenge is that it never modifies
//    pointers in the 'main' registry, so global FLAG_* vars always
//    point to the right place.
// --------------------------------------------------------------------

class FlagSaverImpl {
 public:
  // Constructs an empty FlagSaverImpl object.
  FlagSaverImpl() {}
  ~FlagSaverImpl() {
    // reclaim memory from each of our CommandLineFlags
    for (const SavedFlag& src : backup_registry_) {
      Delete(src.op, src.current);
      Delete(src.op, src.default_value);
    }
  }

  // Saves the flag states from the flag registry into this object.
  // It's an error to call this more than once.
  // Must be called when the registry mutex is not held.
  void SaveFromRegistry() {
    assert(backup_registry_.empty());  // call only once!
    SavedFlag saved;
    flags_internal::ForEachFlag([&](flags_internal::CommandLineFlag* flag) {
      if (flag->IsRetired()) return;

      saved.name = flag->Name();
      saved.op = flag->op;
      saved.marshalling_op = flag->marshalling_op;
      {
        absl::MutexLock l(flag->InitFlagIfNecessary());
        saved.validator = flag->validator;
        saved.modified = flag->modified;
        saved.on_command_line = flag->on_command_line;
        saved.current = Clone(saved.op, flag->cur);
        saved.default_value = Clone(saved.op, flag->def);
        saved.counter = flag->counter;
      }
      backup_registry_.push_back(saved);
    });
  }

  // Restores the saved flag states into the flag registry.  We
  // assume no flags were added or deleted from the registry since
  // the SaveFromRegistry; if they were, that's trouble!  Must be
  // called when the registry mutex is not held.
  void RestoreToRegistry() {
    FlagRegistry* const global_registry = FlagRegistry::GlobalRegistry();
    FlagRegistryLock frl(global_registry);
    for (const SavedFlag& src : backup_registry_) {
      CommandLineFlag* flag = global_registry->FindFlagLocked(src.name);
      // If null, flag got deleted from registry.
      if (!flag) continue;

      bool restored = false;
      {
        absl::MutexLock l(flag->InitFlagIfNecessary());
        flag->validator = src.validator;
        flag->modified = src.modified;
        flag->on_command_line = src.on_command_line;
        if (flag->counter != src.counter ||
            ChangedDirectly(flag, src.default_value, flag->def)) {
          restored = true;
          Copy(src.op, src.default_value, flag->def);
        }
        if (flag->counter != src.counter ||
            ChangedDirectly(flag, src.current, flag->cur)) {
          restored = true;
          Copy(src.op, src.current, flag->cur);
          UpdateCopy(flag);
          flag->InvokeCallback();
        }
      }

      if (restored) {
        flag->counter++;

        // Revalidate the flag because the validator might store state based
        // on the flag's value, which just changed due to the restore.
        // Failing validation is ignored because it's assumed that the flag
        // was valid previously and there's little that can be done about it
        // here, anyway.
        flag->ValidateInputValue(flag->CurrentValue());

        ABSL_INTERNAL_LOG(
            INFO, absl::StrCat("Restore saved value of ", flag->Name(), ": ",
                               Unparse(src.marshalling_op, src.current)));
      }
    }
  }

 private:
  struct SavedFlag {
    absl::string_view name;
    FlagOpFn op;
    FlagMarshallingOpFn marshalling_op;
    int64_t counter;
    bool modified;
    bool on_command_line;
    bool (*validator)();
    const void* current;        // nullptr after restore
    const void* default_value;  // nullptr after restore
  };

  std::vector<SavedFlag> backup_registry_;

  FlagSaverImpl(const FlagSaverImpl&);  // no copying!
  void operator=(const FlagSaverImpl&);
};

FlagSaver::FlagSaver() : impl_(new FlagSaverImpl()) {
  impl_->SaveFromRegistry();
}

void FlagSaver::Ignore() {
  delete impl_;
  impl_ = nullptr;
}

FlagSaver::~FlagSaver() {
  if (!impl_) return;

  impl_->RestoreToRegistry();
  delete impl_;
}

// --------------------------------------------------------------------
// GetAllFlags()
//    The main way the FlagRegistry class exposes its data.  This
//    returns, as strings, all the info about all the flags in
//    the main registry, sorted first by filename they are defined
//    in, and then by flagname.
// --------------------------------------------------------------------

struct FilenameFlagnameLess {
  bool operator()(const CommandLineFlagInfo& a,
                  const CommandLineFlagInfo& b) const {
    int cmp = absl::string_view(a.filename).compare(b.filename);
    if (cmp != 0) return cmp < 0;
    return a.name < b.name;
  }
};

void FillCommandLineFlagInfo(CommandLineFlag* flag,
                             CommandLineFlagInfo* result) {
  result->name = std::string(flag->Name());
  result->type = std::string(flag->Typename());
  result->description = flag->Help();
  result->filename = flag->Filename();

  if (!flag->IsAbseilFlag()) {
    if (!flag->IsModified() && ChangedDirectly(flag, flag->cur, flag->def)) {
      flag->modified = true;
    }
  }

  result->current_value = flag->CurrentValue();
  result->default_value = flag->DefaultValue();
  result->is_default = !flag->IsModified();
  result->has_validator_fn = flag->HasValidatorFn();
  absl::MutexLock l(flag->InitFlagIfNecessary());
  result->flag_ptr = flag->IsAbseilFlag() ? nullptr : flag->cur;
}

// --------------------------------------------------------------------

CommandLineFlag* FindCommandLineFlag(absl::string_view name) {
  if (name.empty()) return nullptr;
  FlagRegistry* const registry = FlagRegistry::GlobalRegistry();
  FlagRegistryLock frl(registry);

  return registry->FindFlagLocked(name);
}

CommandLineFlag* FindRetiredFlag(absl::string_view name) {
  FlagRegistry* const registry = FlagRegistry::GlobalRegistry();
  FlagRegistryLock frl(registry);

  return registry->FindRetiredFlagLocked(name);
}

// --------------------------------------------------------------------

void ForEachFlagUnlocked(std::function<void(CommandLineFlag*)> visitor) {
  FlagRegistry* const registry = FlagRegistry::GlobalRegistry();
  for (FlagRegistry::FlagConstIterator i = registry->flags_.begin();
       i != registry->flags_.end(); ++i) {
    visitor(i->second);
  }
}

void ForEachFlag(std::function<void(CommandLineFlag*)> visitor) {
  FlagRegistry* const registry = FlagRegistry::GlobalRegistry();
  FlagRegistryLock frl(registry);
  ForEachFlagUnlocked(visitor);
}

// --------------------------------------------------------------------

void GetAllFlags(std::vector<CommandLineFlagInfo>* OUTPUT) {
  flags_internal::ForEachFlag([&](CommandLineFlag* flag) {
    if (flag->IsRetired()) return;

    CommandLineFlagInfo fi;
    FillCommandLineFlagInfo(flag, &fi);
    OUTPUT->push_back(fi);
  });

  // Now sort the flags, first by filename they occur in, then alphabetically
  std::sort(OUTPUT->begin(), OUTPUT->end(), FilenameFlagnameLess());
}

// --------------------------------------------------------------------

bool RegisterCommandLineFlag(CommandLineFlag* flag) {
  FlagRegistry::GlobalRegistry()->RegisterFlag(flag);
  return true;
}

// --------------------------------------------------------------------

bool Retire(FlagOpFn ops, FlagMarshallingOpFn marshalling_ops,
            const char* name) {
  auto* flag = new CommandLineFlag(
      name,
      /*help_text=*/absl::flags_internal::HelpText::FromStaticCString(nullptr),
      /*filename_arg=*/"RETIRED", ops, marshalling_ops,
      /*initial_value_gen=*/nullptr,
      /*retired_arg=*/true, nullptr, nullptr);
  FlagRegistry::GlobalRegistry()->RegisterFlag(flag);
  return true;
}

// --------------------------------------------------------------------

bool IsRetiredFlag(absl::string_view name, bool* type_is_bool) {
  assert(!name.empty());
  CommandLineFlag* flag = flags_internal::FindRetiredFlag(name);
  if (flag == nullptr) {
    return false;
  }
  assert(type_is_bool);
  *type_is_bool = flag->IsOfType<bool>();
  return true;
}

}  // namespace flags_internal
}  // namespace absl
