// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

// Controller object to keep track of a file selection flow in a WebState. It is
// created when the user triggers a file input in a web page, and lives until
// the selection is submitted or canceled.
class ChooseFileController {
 public:
  // Delegate interface for `ChooseFileController`.
  struct Delegate {
    // Called when `controller` submitted a file selection.
    virtual void DidSubmitSelection(ChooseFileController* controller,
                                    NSArray<NSURL*>* file_urls,
                                    NSString* display_string,
                                    UIImage* icon_image) = 0;
  };
  // Observer interface for `ChooseFileController`.
  struct Observer : public base::CheckedObserver {
    // Called when the `controller` is being destroyed.
    virtual void ChooseFileControllerDestroyed(
        ChooseFileController* controller) = 0;
  };

  explicit ChooseFileController(ChooseFileEvent event);
  ChooseFileController(const ChooseFileController&) = delete;
  ChooseFileController(ChooseFileController&&) = delete;
  virtual ~ChooseFileController();

  ChooseFileController& operator=(const ChooseFileController&) = delete;
  ChooseFileController& operator=(ChooseFileController&&) = delete;

  // Sets `delegate` as delegate.
  void SetDelegate(Delegate* delegate);

  // Add/Remove `observer` to/from the list of observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the event associated with this controller.
  const ChooseFileEvent& GetChooseFileEvent() const;

  // Submit file selection. This can only be called once.
  void SubmitSelection(NSArray<NSURL*>* file_urls,
                       NSString* display_string,
                       UIImage* icon_image);
  // Returns whether `SubmitSelection()` has been called.
  bool HasSubmittedSelection() const;

  // Returns whether a file picker is presented.
  virtual bool IsPresentingFilePicker() const = 0;
  // Sets whether a file picker is presented. This should be set to true before
  // presenting a UI which will let the user select files to be submitted.
  virtual void SetIsPresentingFilePicker(bool is_presenting) = 0;

  // Returns whether this controller is expired. If this controller is expired,
  // then calling `SubmitSelection()` is a no-op, and this controller should be
  // destroyed.
  virtual bool HasExpired() const = 0;

  // Abort the Choose file selection flow by calling the registered handler.
  // Is no-op if a selection has been submitted.
  virtual void Abort();

  // Sets the abort handler that can be called to abort the flow.
  virtual void SetAbortHandler(base::OnceClosure abort_handler);

 protected:
  // Actually submits the selection.
  virtual void DoSubmitSelection(NSArray<NSURL*>* file_urls,
                                 NSString* display_string,
                                 UIImage* icon_image) = 0;

 private:
  // Set to true when file selection is submitted or canceled.
  bool selection_submitted_ = false;
  // The file selection event associated with this controller.
  ChooseFileEvent choose_file_event_;
  // A closure to abort the flow.
  base::OnceClosure abort_handler_ = base::DoNothing();
  // Delegate of this controller.
  raw_ptr<Delegate> delegate_ = nullptr;
  // Observers list.
  base::ObserverList<Observer, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_
