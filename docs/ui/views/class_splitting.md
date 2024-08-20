## Views Dialog Class Splitting

This document outlines how to refactor a common old Views design pattern into a
cluster of smaller objects with less individual responsibility that are easier
to test. The result is broadly similar to the model-view-controller paradigm but
with some Views-specific differences.

This document is specifically applicable to dialogs, bubbles, and secondary UI
surfaces that have their own Widgets. The techniques described here work well
for subclasses of:

* `WidgetDelegate`
* `DialogDelegate`
* `BubbleDialogDelegate`

This document generally uses `DialogDelegate` throughout.

### The Old Pattern: DialogDelegateView Controllers

Legacy Views dialogs often have a class like this:

```c++
class MyDialogView : public views::DialogDelegateView,
                     public SomeViewListener,
                     public MyModelListener {
 public:
  MyDialogView(MyModel* model);
  ~MyDialogView() override;

  // DialogDelegate:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ...

  // MyViewListener:
  void OnMyViewClicked(MyView* view) override;

  // MyModelListener:
  void OnMyModelChanged(MyModel* model) override;

 private:
  MyModel* model_;
  Label* status_;

  ...
};

void MyDialogView::OnMyViewClicked(MyView* view) {
  // ... complex business logic ...
  model_->Update(new_state);
}

void MyDialogView::OnModelChanged(Model* model) {
  // ... complex presentation logic ...
}
```

### The Motivation & The Single Responsibility Principle

The single responsibility principle is as follows: each class should have one
responsibility. The class given above has several responsibilities:

* It functions as a `DialogDelegate` for a given `Widget`
* It functions as a `View` within that `Widget`
* It handles translating user actions on the dialog to model updates
* It handles translating model updates into visual changes in the dialog

To make matters worse, classes of this pattern often do this in their
constructor:

```c++
MyDialogView::MyDialogView(MyModel* model) {
  // ... lots of setup ...
  views::DialogDelegate::CreateDialogWidget(this, context, parent)->Show();
}
```

The last line of code of the constructor packs a lot of meaning:

* `CreateDialogWidget` does or does not take ownership of `this`, depending on
  whether `MyDialogView` overrides `DeleteDelegate`
* By default, `DialogDelegateView` uses itself as the contents view of the
  created widget
* The created widget takes ownership of itself, and is shown on screen as a
  side-effect of the constructor

Doing this makes classes of this pattern exceptionally difficult to test.

### The New Pattern: Decomposed Classes

Here's how this class might look in "new style", using callbacks rather than
observer/listener interfaces, and using composition instead of inheritance for
both `View` and `DialogDelegate`:

```c++
class MyDialog {
 public:
  MyDialog(MyModel* model);
  ~MyDialog();

  void Show(gfx::NativeWindow context, gfx::NativeView parent);

 private:
  void OnModelChanged(MyModel* model);
  void OnMyViewClicked(MyView* view);

  std::unique_ptr<View> MakeContentsView();
  std::unique_ptr<DialogDelegate> MakeDialogDelegate();

  base::CallbackListSubscription model_subscription_;

  std::unique_ptr<DialogDelegate> delegate_;

  Widget* widget_ = nullptr;  // if needed
  const Model* model_;    // usually needed

  Label* status_ = nullptr;   // or similar Views that are needed later
};

MyDialog::MyDialog(MyModel* model) : model_(model) {
  model_subscription_ = model->RegisterUpdateCallback(
      base::BindRepeating(&MyDialog::OnModelChanged, base::Unretained(this)));

  delegate_ = MakeDialogDelegate(MakeContentsView());
}

void MyDialog::Show(gfx::NativeWindow context, gfx::NativeView parent) {
  widget_ = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate_), context, parent);
  widget_->Show();
  // Or if we don't need to store widget_ we could just do:
  views::DialogDelegate::CreateDialogWidget(
      std::move(delegate_), context, parent)->Show();
}

std::unique_ptr<View> MyDialog::MakeContentsView() {
  // Create the contents view, set up its LayoutManager, create any needed
  // subviews, and store weak pointers to them. For example:
  auto contents = std::make_unique<BoxLayoutView>();

  auto status = std::make_unique<Label>();
  status->ConfigureAsDesired();
  status_ = contents->AddChildView(std::move(status));

  // We don't need a reference to this view after creation time so we don't
  // bother storing it:
  auto help = std::make_unique<MyView>(
      base::BindRepeating(&MyDialog::OnMyViewClicked, base::Unretained(this)));
  contents->AddChildView(std::move(help));
}

std::unique_ptr<DialogDelegate> MyDialog::MakeDialogDelegate(
    std::unique_ptr<View> contents) {
  // Create a DialogDelegate and configure it as needed:
  auto delegate = std::make_unique<DialogDelegate>();
  delegate->SetContentsView(std::move(contents));
  delegate->SetShowCloseButton(...);
  delegate->SetTitle(...);
  return delegate;
}
```

Now `MyDialog` has a single responsibility: it ties together a `DialogDelegate`,
a `View`, and a `MyModel`. The ownership of `MyDialog` is now very clear:
`MyDialog` is always owned by the client regardless of what happens with the
underlying `Widget`. The created `DialogDelegate` is owned by `MyDialog` unless
and until it is handed off to a `Widget`. The contents view (and hence all the
other views) are owned by the `DialogDelegate` until they are handed off (by
`DialogDelegate` itself) to the `Widget`'s `RootView`.

### Going Stateless

Some dialogs don't actually have any meaningful internal state. For example,
let's suppose we are displaying a dialog to the user that prompts them for a
text string, then calls a method on a given controller object with that text
string.

In the "old style" that class might look like:

```c++
class MyPromptView : public DialogDelegateView {
 public:
  static void Show(MyController* controller,
                   gfx::NativeWindow context,
                   gfx::NativeView parent);

 private:
  MyPromptView(MyController* controller);
  ~MyPromptView() override;

  void OnDialogAccepted();

  Textfield* field_;
  MyController* controller_;
};

// static
void MyPromptView::Show(MyController* controller, gfx::NativeWindow context,
                        gfx::NativeView parent) {
  views::DialogDelegate::CreateDialogView(
      new MyPromptView(controller), context, parent)->Show();
}

MyPromptView::MyPromptView(MyController* controller) : controller_(controller) {
  SetDialogButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetAcceptCallback(base::BindOnce(
      &MyPromptView::OnDialogAccepted, base::Unretained(this)));

  SetLayoutManager(...);
  field_ = AddChildView(std::make_unique<Textfield>(...));
  AddChildView(std::make_unique<Label>(...));
}

void MyPromptView::OnDialogAccepted() {
  controller_->OnUserEnteredText(field_->GetText());
}
```

Note that actually, the entire public interface of `MyPromptView` is a single
static method, so if we use composition instead, we get:

### The Stateless Way

```c++
namespace {

void OnDialogAccepted(MyController* controller, Textfield* field) {
  controller->OnUserEnteredText(field->GetText());
}

std::unique_ptr<DialogDelegate> MakePromptDialog(MyController* controller) {
  auto contents = std::make_unique<View>(...);
  contents->SetLayoutManager(...);

  auto* field = contents->AddChildView(std::make_unique<Textfield>(...));
  contents->AddChildView(std::make_unique<Label>(...));

  auto delegate = std::make_unique<DialogDelegate>(...);
  delegate->SetAcceptCallback(base::BindOnce(&OnDialogAccepted,
      base::Unretained(controller), base::Unretained(field)));
  return delegate;
}

}

void ShowPromptDialog(MyController* controller, gfx::NativeWindow context,
                      gfx::NativeView parent) {
  DialogDelegate::CreateDialogWidget(MakePromptDialog(controller),
                                     context, parent)->Show();
}
```

... the entire dialog class vanishes in a puff of refactoring, replaced by a
single function!

### Refactoring step-by-step

Let's say we are refactoring `MyDialogDelegateView` and want to produce
`MyDialog`.  Here we'll assume `MyDialogDelegateView` subclasses
`DialogDelegateView`, but these steps work with any other `WidgetDelegate`
subclass.

1. Replace every override of a `DialogDelegate` method with a call to one of the
   new setter methods from that class. This can require some care, since the
   values of getters may depend on state that is not available until after
   construction time.
2. Have `MyDialogDelegateView` construct a separate `DialogDelegate` as needed,
   possibly storing a reference to it for later use if needed. Migrate all the
   setup code from (1) to target that new, separate delegate instead. Have
   `MyDialogDelegateView` retain ownership of this new `DialogDelegate` member.
3. Have any code that uses `MyDialogDelegateView` as a `DialogDelegate` instead
   use the new `DialogDelegate` member of that class.
4. Make `MyDialogDelegateView` not inherit from `DialogDelegate`, and rename it
   to `MyDialogView` (inheriting from View rather than `DialogDelegateView`).
   Since `MyDialogView` is still used as the `DialogDelegate`'s contents view,
   instances of `MyDialogView` will end up owned by `Views`.
5. Have `MyDialogView` construct a separate `View` to be the dialog's contents
   view, rather than using itself. Change all calls to `View` methods on
   `MyDialogView` to instead target the new contents view member, and have all
   external users that treat `MyDialogView` as a `View` instead use that View
   member.
6. Make `MyDialogView` not inherit from `View`, and rename it to `MyDialog`.
   Note that instances of `MyDialog` are not owned by anyone now, which is bad.
   If the class still needs to exist at all, give it ownership semantics;
   otherwise, it may be possible to refactor the `MyDialog` class away entirely
   into a single function that creates the `DialogDelegate` and `View` members,
   sets up the `Widget`, and returns.
