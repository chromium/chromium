---
name: ios/extract-complex-inline-callbacks-to-methods
description: When binding callbacks or structuring function subroutines, extract complex lambdas, local blocks, or multi-step logic into dedicated member methods or anonymous-namespace functions
rubric: design
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
---

**Rule.** Extract multi-step, multi-line, or branching logic (such as loops,
conditionals, or repetitive statements) from inline lambdas—whether passed to
base::BindOnce, base::BindRepeating, base::BarrierCallback, or defined as local
lambdas within a function body—into dedicated private member methods or
anonymous-namespace helper functions whenever the lambda captures state (`this`,
`[&]`, `[=]`, raw pointers, or other sequence-bound objects) or grows beyond a
few lines. When the callback or subroutine accesses class state or requires
object lifetime protection, bind directly to a private member method
(&MyClass::Method) using weak_factory\_.GetWeakPtr() or base::WrapRefCounted
instead of capturing loose raw pointers. For stateless helpers or reusable
subroutines, define a non-member function in an unnamed namespace with explicit
parameters, and document its behavior and invariants with doc comments. Avoid
inline closures that return bare or opaque constants ([](<>) { return true; }),
and avoid using local lambdas as inline helper subroutines within a function
body. Short captureless, stateless lambdas that only transform their owned or
copied arguments and touch no `this`/`self`—such as a short loop over moved-in
inputs bound for `PostTaskAndReply`—may stay inline, as may trivial argument
forwarding or transparent adapters.

**Why.** Inlining multi-step, branching, or repetitive logic inside
base::BindOnce/base::BindRepeating calls or local lambdas obscures caller
control flow, bloats enclosing methods, and hides parameter dependencies behind
variable capture semantics ([&]). Passing multiple bound arguments to inline
closures also forces redundant parameter declarations across signatures and
bound argument lists. Furthermore, anonymous lambdas returning bare constants
mask semantic intent. Factoring logic into dedicated named member methods or
file-scope functions makes parameter passing explicit, clarifies asynchronous
contracts, integrates cleanly with object lifetime management via weak pointers
or ref-counting, improves testability and stack trace readability, and keeps
enclosing function bodies concise. A short captureless lambda that only
transforms its moved-in inputs has none of these costs—its inputs are explicit
in the bound arguments and it cannot dangle on `this`—so extracting it adds
indirection without benefit.

## Examples

Bad:

```cpp
// Variant 1: Inlining multi-step callbacks in base::BindOnce or returning opaque constants.
auto service = std::make_unique<Service>(
    channel, base::BindRepeating([]() { return true; }));
ShowDialog(base::BindOnce(
    [](const GURL& url, base::WeakPtr<ProfileIOS> profile, Callback cb) {
      if (profile) ReportCopy(url, profile.get());
      std::move(cb).Run();
    },
    url, profile_->AsWeakPtr(), std::move(cb)));

// Variant 2: Defining local lambdas or blocks as subroutines within a function.
void SaveThemeSpecifics(PrefService* pref_service,
                        const std::string& encoded_theme) {
  auto set_or_clear_pref = [&](std::string_view pref_name) {
    if (encoded_theme.empty()) {
      pref_service->ClearPref(pref_name);
    } else {
      pref_service->SetString(pref_name, encoded_theme);
    }
  };
  set_or_clear_pref(prefs::kIosSavedThemeSpecificsIos);
  set_or_clear_pref(prefs::kIosNtpThemeSpecifics);
}
```

Good:

```cpp
// Variant 1: Extract constants and stateful callbacks to methods with lifetime protection.
namespace {
bool IsGeminiEnabled() { return false; }
}  // namespace
auto service = std::make_unique<Service>(
    channel, base::BindRepeating(&IsGeminiEnabled));
ShowDialog(base::BindOnce(&DialogController::OnCopyReported,
                           weak_factory_.GetWeakPtr(), url, std::move(cb)));

// Variant 2: Extract repetitive subroutines to anonymous-namespace helpers with explicit parameters.
namespace {
void SetOrClearStringPref(PrefService* pref_service,
                          std::string_view pref_name,
                          const std::string& value) {
  if (value.empty()) {
    pref_service->ClearPref(pref_name);
  } else {
    pref_service->SetString(pref_name, value);
  }
}
}  // namespace

void SaveThemeSpecifics(PrefService* pref_service,
                        const std::string& encoded_theme) {
  SetOrClearStringPref(pref_service, prefs::kIosSavedThemeSpecificsIos,
                       encoded_theme);
  SetOrClearStringPref(pref_service, prefs::kIosNtpThemeSpecifics,
                       encoded_theme);
}

// Variant 3: A short captureless lambda that only transforms moved-in inputs
// on a background sequence may stay inline.
_taskRunner->PostTaskAndReply(
    FROM_HERE,
    base::BindOnce(
        [](std::vector<base::FilePath> paths) {
          for (const base::FilePath& path : paths) {
            base::DeletePathRecursively(path);
          }
        },
        std::move(paths)),
    std::move(callback));
```
