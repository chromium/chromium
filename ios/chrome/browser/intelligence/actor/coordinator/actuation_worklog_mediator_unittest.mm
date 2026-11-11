// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* const kTaskTitle = @"Task Title";

// Creates a chip titled `text`.
ActuationWorklogChip* ChipWithText(NSString* text) {
  return [[ActuationWorklogChip alloc] initWithText:text icon:nil];
}

// Creates an entry titled `title`, with `chip`.
ActuationWorklogEntry* Entry(NSString* title,
                             ActuationWorklogChip* chip = nil) {
  return [[ActuationWorklogEntry alloc]
      initWithItem:[ActuationWorklogItem simpleItemWithTitle:title active:YES]
              chip:chip];
}

// Creates an intervention with a single action.
ActuationInterventionData* CreateIntervention() {
  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:@"Continue"];
  return [ActuationInterventionData rowItemWithActions:@[ action ]];
}

}  // namespace

// Records what the worklog and its suggestions display.
@interface FakeActuationWorklogConsumer
    : NSObject <ActuationSuggestionsConsumer, ActuationWorklogConsumer>
@property(nonatomic, assign) BOOL actuationActive;
@property(nonatomic, strong)
    ActuationHeaderContentVariants* headerContentVariants;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogItem*>* items;
// Whether each item of `items` was added with an animation.
@property(nonatomic, strong) NSMutableArray<NSNumber*>* animated;
@property(nonatomic, strong) ActuationWorklogChip* chip;
@property(nonatomic, strong) ActuationInterventionData* intervention;
@property(nonatomic, assign) NSUInteger resetCount;
@property(nonatomic, strong) NSArray<ActorFormSuggestion*>* suggestions;
@property(nonatomic, strong) ActuationWorklogItem* suggestionsItem;
@end

@implementation FakeActuationWorklogConsumer

- (instancetype)init {
  if ((self = [super init])) {
    _items = [NSMutableArray array];
    _animated = [NSMutableArray array];
  }
  return self;
}

- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated {
  [_items addObject:item];
  [_animated addObject:@(animated)];
  _chip = chip;
}

- (void)setLastItem:(ActuationWorklogItem*)item
               chip:(ActuationWorklogChip*)chip {
  _items[_items.count - 1] = item;
  _chip = chip;
}

- (void)reset {
  _resetCount++;
  [_items removeAllObjects];
  [_animated removeAllObjects];
  _chip = nil;
  _headerContentVariants = nil;
  _intervention = nil;
}

- (void)setSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    forActiveWorklogItem:(ActuationWorklogItem*)worklogItem {
  _suggestions = suggestions;
  _suggestionsItem = worklogItem;
}

@end

// Publishes the states set by the tests, and records the user actions.
@interface FakeActorTaskDataSource
    : NSObject <ActorTaskActionHandler, ActorTaskDataSource>
// Setting it publishes it to `observer`.
@property(nonatomic, strong) ActuationWorklogState* state;
@property(nonatomic, weak, readonly) id<ActorTaskDataSourceObserver> observer;
@property(nonatomic, strong, readonly)
    ActuationInterventionAction* resolvedAction;
@property(nonatomic, strong, readonly) ActorFormSuggestion* selectedSuggestion;
@property(nonatomic, assign, readonly) BOOL taskStopped;
@end

@implementation FakeActorTaskDataSource

- (void)setState:(ActuationWorklogState*)state {
  _state = state;
  [_observer taskDataSource:self didUpdateState:state];
}

- (void)addObserver:(id<ActorTaskDataSourceObserver>)observer {
  _observer = observer;
  [observer taskDataSource:self didUpdateState:_state];
}

- (void)removeObserver:(id<ActorTaskDataSourceObserver>)observer {
  if (_observer == observer) {
    _observer = nil;
  }
}

- (void)resolveInterventionWithAction:(ActuationInterventionAction*)action {
  _resolvedAction = action;
}

- (void)selectSuggestion:(ActorFormSuggestion*)suggestion {
  _selectedSuggestion = suggestion;
}

- (void)stopTask {
  _taskStopped = YES;
}

@end

namespace {

class ActuationWorklogMediatorTest : public PlatformTest {
 protected:
  ActuationWorklogMediatorTest() {
    ActuationHeaderContent* content =
        [[ActuationHeaderContent alloc] initWithTitle:kTaskTitle subtitle:nil];
    header_content_ = [[ActuationHeaderContentVariants alloc]
        initWithMinimizedContent:content
                  compactContent:content
                 expandedContent:content];
    mediator_.consumer = consumer_;
    mediator_.suggestionsConsumer = consumer_;
  }

  ~ActuationWorklogMediatorTest() override { [mediator_ disconnect]; }

  // Publishes the state of an active task displaying `entries`, with
  // `intervention`, and `suggestions` for the last entry.
  void Publish(NSArray<ActuationWorklogEntry*>* entries,
               ActuationInterventionData* intervention = nil,
               NSArray<ActorFormSuggestion*>* suggestions = nil) {
    data_source_.state = [[ActuationWorklogState alloc]
        initWithActuationActive:YES
                  headerContent:header_content_
                        entries:entries
                   intervention:intervention
                    suggestions:suggestions
               suggestionsEntry:(suggestions ? entries.lastObject : nil)];
  }

  void StartObserving() {
    [mediator_ startObservingDataSource:data_source_
                          actionHandler:data_source_];
  }

  ActuationHeaderContentVariants* header_content_;
  FakeActuationWorklogConsumer* consumer_ =
      [[FakeActuationWorklogConsumer alloc] init];
  FakeActorTaskDataSource* data_source_ =
      [[FakeActorTaskDataSource alloc] init];
  ActuationWorklogMediator* mediator_ = [[ActuationWorklogMediator alloc] init];
};

// Tests that the state published before observing is displayed without
// animation, with the chip of the latest entry.
TEST_F(ActuationWorklogMediatorTest, TestCurrentStateIsDisplayed) {
  ActuationWorklogChip* chip = ChipWithText(@"Chip");
  NSArray<ActuationWorklogEntry*>* entries =
      @[ Entry(@"A"), Entry(@"B", chip) ];
  Publish(entries);

  StartObserving();

  EXPECT_TRUE(consumer_.actuationActive);
  EXPECT_EQ(consumer_.headerContentVariants, header_content_);
  EXPECT_NSEQ(consumer_.items, [entries valueForKey:@"item"]);
  EXPECT_NSEQ(consumer_.animated, (@[ @NO, @NO ]));
  EXPECT_EQ(consumer_.chip, chip);
}

// Tests that nothing is displayed until a state is published, and that the
// first state isn't animated.
TEST_F(ActuationWorklogMediatorTest, TestFirstPublishedStateIsDisplayed) {
  StartObserving();
  EXPECT_EQ(consumer_.headerContentVariants, nil);
  EXPECT_EQ(consumer_.items.count, 0u);

  ActuationWorklogEntry* a = Entry(@"A");
  Publish(@[ a ]);

  EXPECT_EQ(consumer_.headerContentVariants, header_content_);
  EXPECT_NSEQ(consumer_.items, @[ a.item ]);
  EXPECT_NSEQ(consumer_.animated, @[ @NO ]);
}

// Tests that the entries appended by a state, even several at once, are added
// with an animation, and that the chip follows the latest entry.
TEST_F(ActuationWorklogMediatorTest, TestAppendedEntriesAreAnimated) {
  ActuationWorklogEntry* a = Entry(@"A");
  Publish(@[ a ]);
  StartObserving();
  ActuationWorklogChip* chip = ChipWithText(@"Chip");
  NSArray<ActuationWorklogEntry*>* entries =
      @[ a, Entry(@"B"), Entry(@"C", chip) ];

  Publish(entries);
  EXPECT_NSEQ(consumer_.items, [entries valueForKey:@"item"]);
  EXPECT_NSEQ(consumer_.animated, (@[ @NO, @YES, @YES ]));
  EXPECT_EQ(consumer_.chip, chip);

  // An entry without chip hides the chip of the previous one.
  Publish([entries arrayByAddingObject:Entry(@"D")]);
  EXPECT_EQ(consumer_.chip, nil);
  EXPECT_EQ(consumer_.resetCount, 0u);
}

// Tests that a state that doesn't change the entries doesn't send them again.
TEST_F(ActuationWorklogMediatorTest, TestUnchangedEntriesAreNotSentAgain) {
  NSArray<ActuationWorklogEntry*>* entries = @[ Entry(@"A") ];
  Publish(entries);
  StartObserving();

  Publish(entries, CreateIntervention());

  EXPECT_NSEQ(consumer_.items, [entries valueForKey:@"item"]);
  EXPECT_NSEQ(consumer_.animated, @[ @NO ]);
  EXPECT_EQ(consumer_.resetCount, 0u);
}

// Tests that a state replacing the last entry and appending another updates the
// worklog in place.
TEST_F(ActuationWorklogMediatorTest, TestReplacedLastEntryIsUpdatedInPlace) {
  ActuationWorklogEntry* a = Entry(@"A");
  Publish(@[ a, Entry(@"B") ]);
  StartObserving();
  NSArray<ActuationWorklogEntry*>* entries = @[ a, Entry(@"B2"), Entry(@"C") ];

  Publish(entries);

  EXPECT_EQ(consumer_.resetCount, 0u);
  EXPECT_NSEQ(consumer_.items, [entries valueForKey:@"item"]);
  EXPECT_NSEQ(consumer_.animated, (@[ @NO, @NO, @YES ]));
}

// Tests that a state removing an entry displays the whole state again without
// animation.
TEST_F(ActuationWorklogMediatorTest, TestRemovedEntryRedisplaysState) {
  ActuationWorklogChip* chip = ChipWithText(@"Chip");
  ActuationWorklogEntry* a = Entry(@"A", chip);
  ActuationInterventionData* intervention = CreateIntervention();
  Publish(@[ a, Entry(@"B") ], intervention);
  StartObserving();

  Publish(@[ a ], intervention);

  EXPECT_EQ(consumer_.resetCount, 1u);
  EXPECT_EQ(consumer_.headerContentVariants, header_content_);
  EXPECT_NSEQ(consumer_.items, @[ a.item ]);
  EXPECT_NSEQ(consumer_.animated, @[ @NO ]);
  EXPECT_EQ(consumer_.chip, chip);
  EXPECT_EQ(consumer_.intervention, intervention);
}

// Tests that a state replacing an entry other than the last displays the
// entries again without animation.
TEST_F(ActuationWorklogMediatorTest, TestReplacedEarlierEntryRedisplaysState) {
  ActuationWorklogEntry* a = Entry(@"A");
  ActuationWorklogEntry* c = Entry(@"C");
  Publish(@[ a, Entry(@"B"), c ]);
  StartObserving();
  NSArray<ActuationWorklogEntry*>* entries = @[ a, Entry(@"B2"), c ];

  Publish(entries);

  EXPECT_EQ(consumer_.resetCount, 1u);
  EXPECT_NSEQ(consumer_.items, [entries valueForKey:@"item"]);
  EXPECT_NSEQ(consumer_.animated, (@[ @NO, @NO, @NO ]));
}

// Tests that the intervention and the suggestions of each state are forwarded,
// the suggestions with the item of their entry.
TEST_F(ActuationWorklogMediatorTest,
       TestInterventionAndSuggestionsAreForwarded) {
  NSArray<ActuationWorklogEntry*>* entries = @[ Entry(@"A"), Entry(@"Card") ];
  ActuationInterventionData* intervention = CreateIntervention();
  NSArray<ActorFormSuggestion*>* suggestions = @[ [[ActorFormSuggestion alloc]
      initWithCredential:actor_login::Credential()] ];
  StartObserving();

  Publish(entries, intervention, suggestions);
  EXPECT_EQ(consumer_.intervention, intervention);
  EXPECT_NSEQ(consumer_.suggestions, suggestions);
  EXPECT_EQ(consumer_.suggestionsItem, entries.lastObject.item);

  Publish(entries);
  EXPECT_EQ(consumer_.intervention, nil);
  EXPECT_EQ(consumer_.suggestions, nil);
  EXPECT_EQ(consumer_.suggestionsItem, nil);
}

// Tests that stopping observation detaches from the data source and resets the
// consumer.
TEST_F(ActuationWorklogMediatorTest, TestStopObservingResetsConsumer) {
  Publish(@[ Entry(@"A") ]);
  StartObserving();

  [mediator_ stopObservingDataSource];

  EXPECT_EQ(data_source_.observer, nil);
  EXPECT_FALSE(consumer_.actuationActive);
  EXPECT_EQ(consumer_.headerContentVariants, nil);
  EXPECT_EQ(consumer_.items.count, 0u);
}

// Tests that observing the observed data source again is a no-op.
TEST_F(ActuationWorklogMediatorTest, TestStartObservingSameDataSourceIsNoOp) {
  Publish(@[ Entry(@"A") ]);
  StartObserving();

  StartObserving();

  EXPECT_EQ(consumer_.resetCount, 0u);
  EXPECT_EQ(consumer_.items.count, 1u);
}

// Tests that observing a new data source replaces the displayed state with its
// own.
TEST_F(ActuationWorklogMediatorTest,
       TestStartObservingNewDataSourceReplacesPrevious) {
  Publish(@[ Entry(@"A") ]);
  StartObserving();
  FakeActorTaskDataSource* previous_data_source = data_source_;
  data_source_ = [[FakeActorTaskDataSource alloc] init];
  ActuationWorklogEntry* b = Entry(@"B");
  Publish(@[ b ]);

  StartObserving();

  EXPECT_EQ(previous_data_source.observer, nil);
  EXPECT_NSEQ(consumer_.items, @[ b.item ]);
  EXPECT_NSEQ(consumer_.animated, @[ @NO ]);
}

// Tests that the user actions are forwarded to the action handler.
TEST_F(ActuationWorklogMediatorTest, TestUserActionsAreForwarded) {
  StartObserving();
  ActuationInterventionAction* action =
      CreateIntervention().actions.firstObject;
  ActorFormSuggestion* suggestion = [[ActorFormSuggestion alloc]
      initWithCredential:actor_login::Credential()];

  [mediator_ didTriggerInterventionAction:action];
  [mediator_ suggestionsCoordinator:nil didSelectSuggestion:suggestion];
  [mediator_ stopActuation];

  EXPECT_EQ(data_source_.resolvedAction, action);
  EXPECT_EQ(data_source_.selectedSuggestion, suggestion);
  EXPECT_TRUE(data_source_.taskStopped);
}

}  // namespace
