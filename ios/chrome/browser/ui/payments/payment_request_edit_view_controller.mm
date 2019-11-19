// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/cells/legacy_autofill_edit_item.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_actions.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kWarningMessageAccessibilityID =
    @"kWarningMessageAccessibilityID";

namespace {

NSString* const kPaymentRequestEditCollectionViewAccessibilityID =
    @"kPaymentRequestEditCollectionViewAccessibilityID";

const CGFloat kSeparatorEdgeInset = 14;

const CGFloat kFooterCellHorizontalPadding = 16;

// Returns the LegacyAutofillEditCell that is the parent view of the
// |textField|.
LegacyAutofillEditCell* AutofillEditCellForTextField(UITextField* textField) {
  for (UIView* view = textField; view; view = [view superview]) {
    LegacyAutofillEditCell* cell =
        base::mac::ObjCCast<LegacyAutofillEditCell>(view);
    if (cell)
      return cell;
  }

  // There has to be a cell associated with this text field.
  NOTREACHED();
  return nil;
}

CollectionViewSwitchCell* CollectionViewSwitchCellForSwitchField(
    UISwitch* switchField) {
  for (UIView* view = switchField; view; view = [view superview]) {
    CollectionViewSwitchCell* cell =
        base::mac::ObjCCast<CollectionViewSwitchCell>(view);
    if (cell)
      return cell;
  }

  // There should be a cell associated with this switch field.
  NOTREACHED();
  return nil;
}

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierFooter,
  SectionIdentifierFirstField,  // Must be the last section identifier.
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeTextField,      // This is a repeated item type.
  ItemTypeSelectorField,  // This is a repeated item type.
  ItemTypeSwitchField,    // This is a repeated item type.
  ItemTypeErrorMessage,   // This is a repeated item type.
};

// Returns an error PaymentTextItem with the specified |errorMessage|.
PaymentsTextItem* ErrorMessageItemForError(NSString* errorMessage) {
  PaymentsTextItem* errorMessageItem =
      [[PaymentsTextItem alloc] initWithType:ItemTypeErrorMessage];
  errorMessageItem.text = errorMessage;
  errorMessageItem.leadingImage = [NativeImage(IDR_IOS_PAYMENTS_WARNING)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  errorMessageItem.leadingImageTintColor = [UIColor colorNamed:kRedColor];
  errorMessageItem.accessibilityIdentifier = kWarningMessageAccessibilityID;
  return errorMessageItem;
}

}  // namespace

@interface PaymentRequestEditViewController () <
    FormInputAccessoryViewDelegate,
    PaymentRequestEditViewControllerActions,
    UIPickerViewDataSource,
    UIPickerViewDelegate,
    UITextFieldDelegate> {
  // The currently focused cell. May be nil.
  __weak LegacyAutofillEditCell* _currentEditingCell;
}

// The accessory view when editing any of text fields.
@property(nonatomic, strong) FormInputAccessoryView* formInputAccessoryView;

// The map of section identifiers to the fields definitions for the editor.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* fields;

// The map of autofill types to UIPickerView options which are arrays of columns
// which themselves are arrays of string rows used for display in UIPickerView.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, NSArray<NSArray<NSString*>*>*>* options;

// The map of autofill types to UIPickerView views.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, UIPickerView*>* pickerViews;

// The field, if any, that is currently being edited. Will return nil if no
// field is currently being edited.
- (EditorField*)currentEditingField;

// Returns the indexPath for the same row as that of |indexPath| in a section
// with the given offset relative to that of |indexPath|. May return nil.
- (NSIndexPath*)indexPathWithSectionOffset:(NSInteger)offset
                                  fromPath:(NSIndexPath*)indexPath;

// Returns the text field with the given offset relative to the currently
// focused text field. May return nil.
- (LegacyAutofillEditCell*)nextTextFieldWithOffset:(NSInteger)offset;

// Enables or disables the accessory view's previous and next buttons depending
// on whether there is a text field before and after the currently focused text
// field.
- (void)updateAccessoryViewButtonsStates;

// Adds an error message item in the section |sectionIdentifier| if
// |errorMessage| is non-empty. Otherwise removes such an item if one exists.
- (void)addOrRemoveErrorMessage:(NSString*)errorMessage
        inSectionWithIdentifier:(NSInteger)sectionIdentifier;

// Validates a specific field. If there is a validation error, displays an error
// message item in the same section as the field and returns NO. Otherwise
// removes the error message item in that section if one exists and sets the
// value on the field.
- (BOOL)validateField:(EditorField*)field;

// Validates each field. If there is a validation error, displays an error
// message item in the same section as the field, sets the focus on the invalid
// textfield, if applicable, and returns NO. Otherwise removes the error message
// item in that section if one exists and sets the value on the field. Returns
// YES if all the fields are validated successfully.
- (BOOL)validateForm;

// Returns whether the given field is valid. Does not update the error message.
- (BOOL)isFieldValid:(EditorField*)field;

// Returns whether all the fields in the form are valid or not. Does not update
// error messages.
- (BOOL)isFormValid;

// Returns the index path for the cell associated with the currently focused
// text field.
- (NSIndexPath*)indexPathForCurrentTextField;

// Returns the associated options for the given UIPickerView.
- (NSArray<NSArray<NSString*>*>*)pickerViewOptionsForPickerView:
    (UIPickerView*)pickerView;

@end

@implementation PaymentRequestEditViewController

@synthesize dataSource = _dataSource;
@synthesize delegate = _delegate;
@synthesize validatorDelegate = _validatorDelegate;
@synthesize fieldsMap = _fieldsMap;
@synthesize fields = _fields;
@synthesize options = _options;
@synthesize pickerViews = _pickerViews;

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self = [self initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    // Set up leading (cancel) button.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(didCancel)];
    [cancelButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
    }
                                forState:UIControlStateDisabled];
    [cancelButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CANCEL)];
    [self navigationItem].leftBarButtonItem = cancelButton;

    // Set up trailing (save) button.
    UIBarButtonItem* saveButton =
        [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_SAVE)
                                         style:UIBarButtonItemStylePlain
                                        target:nil
                                        action:@selector(didSave)];
    [saveButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
    }
                              forState:UIControlStateDisabled];
    [saveButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_SAVE)];
    saveButton.enabled = NO;  // Disabled until form has been validated.
    [self navigationItem].rightBarButtonItem = saveButton;
  }

  return self;
}

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style {
  self = [super initWithLayout:layout style:style];
  if (self) {
    _formInputAccessoryView = [[FormInputAccessoryView alloc] init];
    _options = [[NSMutableDictionary alloc] init];
    _pickerViews = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.formInputAccessoryView setUpWithLeadingView:nil
                                 navigationDelegate:self];

  self.collectionView.accessibilityIdentifier =
      kPaymentRequestEditCollectionViewAccessibilityID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
  self.styler.separatorColor = [UIColor colorNamed:kSeparatorColor];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidShow)
             name:UIKeyboardDidShowNotification
           object:nil];

  // Validate the form so that the first field with an invalid value gets focus.
  if (_dataSource.state == EditViewControllerStateEdit) {
    [self validateForm];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardDidShowNotification
              object:nil];
}

- (EditorField*)currentEditingField {
  if (!_currentEditingCell)
    return nil;

  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  NSInteger sectionIdentifier = [self.collectionViewModel
      sectionIdentifierForSection:[indexPath section]];
  NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
  return self.fieldsMap[key];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  self.title = [_dataSource title];

  [self.pickerViews removeAllObjects];

  CollectionViewItem* headerItem = [_dataSource headerItem];
  if (headerItem) {
    [headerItem setType:ItemTypeHeader];
    [model addSectionWithIdentifier:SectionIdentifierHeader];
    [model addItem:headerItem toSectionWithIdentifier:SectionIdentifierHeader];
  }

  self.fieldsMap =
      [[NSMutableDictionary alloc] initWithCapacity:self.fields.count];

  // Iterate over the fields and add the respective sections and items.
  [self.fields enumerateObjectsUsingBlock:^(EditorField* field,
                                            NSUInteger index, BOOL* stop) {
    NSInteger sectionIdentifier = SectionIdentifierFirstField + index;
    [model addSectionWithIdentifier:sectionIdentifier];
    switch (field.fieldType) {
      case EditorFieldTypeTextField: {
        LegacyAutofillEditItem* item =
            [[LegacyAutofillEditItem alloc] initWithType:ItemTypeTextField];
        item.useScaledFont = YES;
        item.textFieldName = field.label;
        item.textFieldEnabled = field.enabled;
        item.textFieldValue = field.value;
        item.required = field.isRequired;
        item.autofillUIType = field.autofillUIType;
        item.returnKeyType = field.returnKeyType;
        item.keyboardType = field.keyboardType;
        item.autoCapitalizationType = field.autoCapitalizationType;
        item.identifyingIcon = [_dataSource iconIdentifyingEditorField:field];
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
        field.item = item;

        break;
      }
      case EditorFieldTypeSelector: {
        PaymentsSelectorEditItem* item = [[PaymentsSelectorEditItem alloc]
            initWithType:ItemTypeSelectorField];
        item.name = field.label;
        item.value = field.displayValue;
        item.required = field.isRequired;
        item.autofillUIType = field.autofillUIType;
        item.accessibilityTraits |= UIAccessibilityTraitButton;
        item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
        field.item = item;
        break;
      }
      case EditorFieldTypeSwitch: {
        CollectionViewSwitchItem* item =
            [[CollectionViewSwitchItem alloc] initWithType:ItemTypeSwitchField];
        item.useScaledFont = YES;
        item.text = field.label;
        item.on = [field.value boolValue];
        [model addItem:item toSectionWithIdentifier:sectionIdentifier];
        field.item = item;
        break;
      }
      default:
        NOTREACHED();
    }

    field.sectionIdentifier = sectionIdentifier;
    NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
    [self.fieldsMap setObject:field forKey:key];
  }];

  [model addSectionWithIdentifier:SectionIdentifierFooter];
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = l10n_util::GetNSString(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE);
  footerItem.useScaledFont = YES;
  [model addItem:footerItem toSectionWithIdentifier:SectionIdentifierFooter];

  // Validate the non-pristine fields, in order to restore the validation errors
  // that were showing for non-pristine fields. Cannot call
  // [self validateField:...], as that calls |addOrRemoveErrorMessage:...|,
  // which mutates the CollectionView directly. That causes an
  // NSInternalConsistencyException, as the data has not been reloaded from the
  // model yet.
  for (EditorField* field in self.fields) {
    if (!field.isPristine) {
      NSString* errorMessage =
          [_validatorDelegate paymentRequestEditViewController:self
                                                 validateField:field];
      if (errorMessage.length) {
        [model addItem:ErrorMessageItemForError(errorMessage)
            toSectionWithIdentifier:field.sectionIdentifier];
      }
    }
  }

  [self navigationItem].rightBarButtonItem.enabled = [self isFormValid];
}

#pragma mark - PaymentRequestEditConsumer

- (void)setEditorFields:(NSArray<EditorField*>*)fields {
  self.fields = fields;
}

- (void)setOptions:(NSArray<NSArray<NSString*>*>*)options
    forEditorField:(EditorField*)field {
  DCHECK(field.fieldType == EditorFieldTypeTextField);
  LegacyAutofillEditItem* item =
      base::mac::ObjCCastStrict<LegacyAutofillEditItem>(field.item);
  item.textFieldEnabled = field.enabled;
  item.textFieldValue = field.value;

  // Cache the options if there are any and set the text field's UIPickerView.
  if (options.count) {
    NSNumber* key = [NSNumber numberWithInt:field.autofillUIType];
    [self.options setObject:options forKey:key];

    UIPickerView* pickerView = [[UIPickerView alloc] initWithFrame:CGRectZero];
    pickerView.delegate = self;
    pickerView.dataSource = self;
    pickerView.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_pickerView", field.label];
    [self.pickerViews setObject:pickerView forKey:key];
    item.inputView = pickerView;

    [pickerView reloadAllComponents];
    // Set UIPickerView's default selected rows, if possible.
    if (field.value) {
      NSArray<NSString*>* fieldComponents =
          [field.value componentsSeparatedByString:@" / "];
      [options enumerateObjectsUsingBlock:^(NSArray<NSString*>* column,
                                            NSUInteger component, BOOL* stop) {
        DCHECK(component < fieldComponents.count);
        NSUInteger row = [column indexOfObject:fieldComponents[component]];
        if (row != NSNotFound) {
          [pickerView selectRow:row inComponent:component animated:NO];
        }
      }];
    }
  }

  // Reload the item.
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeTextField
                                   sectionIdentifier:field.sectionIdentifier];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  _currentEditingCell = AutofillEditCellForTextField(textField);
  [textField setInputAccessoryView:self.formInputAccessoryView];
  [self updateAccessoryViewButtonsStates];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  DCHECK(_currentEditingCell == AutofillEditCellForTextField(textField));

  // Find the respective editor field, update its value, and validate it.
  EditorField* field = [self currentEditingField];
  DCHECK(field);
  field.value = textField.text;
  field.pristine = NO;
  [self validateField:field];

  [textField setInputAccessoryView:nil];
  _currentEditingCell = nil;

  [self navigationItem].rightBarButtonItem.enabled = [self isFormValid];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK([_currentEditingCell textField] == textField);
  LegacyAutofillEditCell* nextCell = [self nextTextFieldWithOffset:1];
  if (nextCell)
    [nextCell.textField becomeFirstResponder];
  else
    [[_currentEditingCell textField] resignFirstResponder];

  return NO;
}

// This method is called as the text is being typed in, pasted, or deleted.
// Returns NO if the text should be formatted or that the text should only be
// changed via the UIPickerView. Returns YES otherwise. During typing/pasting
// text, |newText| contains one or more new characters. When user deletes text,
// |newText| is empty. |range| is the range of characters to be replaced.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  DCHECK(_currentEditingCell == AutofillEditCellForTextField(textField));

  // Return NO if the respective editor field has an associated UIPickerView.
  // This prevents altering the text unless it is via the UIPickerView.
  EditorField* field = [self currentEditingField];
  DCHECK(field);
  NSNumber* key = [NSNumber numberWithInt:field.autofillUIType];
  if ([self.pickerViews objectForKey:key])
    return NO;

  // Return without formatting the proposed text if no formatting is necessary.
  if (![_dataSource shouldFormatValueForAutofillUIType:field.autofillUIType])
    return YES;

  field.value = [textField.text stringByReplacingCharactersInRange:range
                                                        withString:newText];
  // Format the proposed text.
  field.value =
      [_dataSource formatValue:field.value autofillUIType:field.autofillUIType];

  // Since this method is returning NO, update the text field's value now.
  textField.text = field.value;

  // Get the icon that identifies the field value and reload the cell if the
  // icon changes.
  LegacyAutofillEditItem* item =
      base::mac::ObjCCastStrict<LegacyAutofillEditItem>(field.item);
  UIImage* oldIcon = item.identifyingIcon;
  item.identifyingIcon = [_dataSource iconIdentifyingEditorField:field];
  if (item.identifyingIcon != oldIcon) {
    item.textFieldValue = field.value;
    [self reconfigureCellsForItems:@[ item ]];
  }

  if (!field.isPristine)
    [self validateField:field];
  [self navigationItem].rightBarButtonItem.enabled = [self isFormValid];

  return NO;
}

#pragma mark - FormInputAccessoryViewDelegate

- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender {
  [self moveToAnotherCellWithOffset:1];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  [self moveToAnotherCellWithOffset:-1];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [[_currentEditingCell textField] resignFirstResponder];
}

#pragma mark - UIPickerViewDataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView*)pickerView {
  NSArray<NSArray<NSString*>*>* options =
      [self pickerViewOptionsForPickerView:pickerView];
  return options.count;
}

- (NSInteger)pickerView:(UIPickerView*)pickerView
    numberOfRowsInComponent:(NSInteger)component {
  NSArray<NSArray<NSString*>*>* options =
      [self pickerViewOptionsForPickerView:pickerView];
  DCHECK(component < static_cast<NSInteger>(options.count));
  NSArray<NSString*>* column = options[component];
  return column.count;
}

#pragma mark - UIPickerViewDelegate methods

- (NSString*)pickerView:(UIPickerView*)pickerView
            titleForRow:(NSInteger)row
           forComponent:(NSInteger)component {
  NSArray<NSArray<NSString*>*>* options =
      [self pickerViewOptionsForPickerView:pickerView];
  DCHECK(component < static_cast<NSInteger>(options.count));
  NSArray<NSString*>* column = options[component];
  DCHECK(row < static_cast<NSInteger>(column.count));
  return column[row];
}

- (void)pickerView:(UIPickerView*)pickerView
      didSelectRow:(NSInteger)row
       inComponent:(NSInteger)component {
  DCHECK(_currentEditingCell);

  // Break the current text field value into its components, replace the
  // respective component with the value of the selected row, combine the
  // components, and update the value of the text field.
  NSMutableArray<NSString*>* fieldComponents =
      [[_currentEditingCell.textField.text componentsSeparatedByString:@" / "]
          mutableCopy];
  DCHECK(component < static_cast<NSInteger>(fieldComponents.count));
  fieldComponents[component] =
      [self pickerView:pickerView titleForRow:row forComponent:component];
  _currentEditingCell.textField.text =
      [fieldComponents componentsJoinedByString:@" / "];

  EditorField* field = [self currentEditingField];
  field.value = _currentEditingCell.textField.text;

  // Whenever a picker view changes, this method gets called. As such, it is
  // no longer pristine, and should always be validated. |field.pristine| will
  // be set to NO in -textFieldDidEndEditing:.
  [self validateField:field];
  [self navigationItem].rightBarButtonItem.enabled = [self isFormValid];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeTextField: {
      LegacyAutofillEditCell* autofillEditCell =
          base::mac::ObjCCast<LegacyAutofillEditCell>(cell);
      autofillEditCell.textField.delegate = self;
      autofillEditCell.textField.clearButtonMode = UITextFieldViewModeNever;
      SetUILabelScaledFont(autofillEditCell.textLabel,
                           [MDCTypography body2Font]);
      autofillEditCell.textLabel.textColor =
          [UIColor colorNamed:kTextPrimaryColor];
      SetUITextFieldScaledFont(autofillEditCell.textField,
                               [MDCTypography body1Font]);
      autofillEditCell.textField.textColor = [UIColor colorNamed:kBlueColor];
      break;
    }
    case ItemTypeSwitchField: {
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(switchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeErrorMessage: {
      PaymentsTextCell* errorMessageCell =
          base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
      SetUILabelScaledFont(errorMessageCell.textLabel,
                           [MDCTypography body1Font]);
      errorMessageCell.textLabel.textColor = [UIColor colorNamed:kRedColor];
      break;
    }
    case ItemTypeFooter: {
      CollectionViewFooterCell* footerCell =
          base::mac::ObjCCastStrict<CollectionViewFooterCell>(cell);
      SetUILabelScaledFont(footerCell.textLabel, [MDCTypography body2Font]);
      footerCell.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
      footerCell.textLabel.shadowColor = nil;  // No shadow.
      footerCell.horizontalPadding = kFooterCellHorizontalPadding;
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  // Every field has its own section. Find out which field is selected using
  // the section of |indexPath|. Adjust the index if a header section is
  // present before the editor fields.
  NSInteger index = indexPath.section;
  if ([self.collectionViewModel
          hasSectionForSectionIdentifier:SectionIdentifierHeader]) {
    index--;
  }
  // Early return if the header or the footer sections are selected.
  if (index < 0 || index >= static_cast<NSInteger>(self.fields.count))
    return;

  // Early return if the validation message and not the field is selected.
  if (indexPath.row != 0)
    return;

  EditorField* field = [self.fields objectAtIndex:index];

  // If a selector field is selected, blur the currently focused UITextField.
  // And if a text field is selected, focus the corresponding UITextField.
  if (field.fieldType == EditorFieldTypeSelector) {
    [[_currentEditingCell textField] resignFirstResponder];
  } else if (field.fieldType == EditorFieldTypeTextField) {
    id cell = [collectionView cellForItemAtIndexPath:indexPath];
    // |cell| may be nil if the cell is not visible.
    if (cell) {
      LegacyAutofillEditCell* autofillEditCell =
          base::mac::ObjCCastStrict<LegacyAutofillEditCell>(cell);
      [autofillEditCell.textField becomeFirstResponder];
    }
  }

  if ([self.delegate respondsToSelector:@selector
                     (paymentRequestEditViewController:didSelectField:)]) {
    [_delegate paymentRequestEditViewController:self didSelectField:field];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeHeader:
    case ItemTypeFooter:
    case ItemTypeErrorMessage:
    case ItemTypeTextField:
    case ItemTypeSwitchField:
      return YES;
    default:
      return NO;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeHeader:
      return [_dataSource shouldHideBackgroundForHeaderItem];
    case ItemTypeFooter:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - Helper methods

// Jumps to the cell at the passed offset, If such cell exists.
- (void)moveToAnotherCellWithOffset:(NSInteger)offset {
  LegacyAutofillEditCell* cell = [self nextTextFieldWithOffset:offset];
  if (cell)
    [cell.textField becomeFirstResponder];
}

- (NSIndexPath*)indexPathWithSectionOffset:(NSInteger)offset
                                  fromPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath);
  DCHECK(offset);
  NSInteger nextSection = [indexPath section] + offset;
  if (nextSection >= 0 &&
      nextSection < [[self collectionView] numberOfSections]) {
    return [NSIndexPath indexPathForRow:[indexPath row] inSection:nextSection];
  }
  return nil;
}

- (LegacyAutofillEditCell*)nextTextFieldWithOffset:(NSInteger)offset {
  UICollectionView* collectionView = [self collectionView];
  NSIndexPath* currentCellPath = [self indexPathForCurrentTextField];
  DCHECK(currentCellPath);
  NSIndexPath* nextCellPath =
      [self indexPathWithSectionOffset:offset fromPath:currentCellPath];
  while (nextCellPath) {
    id nextCell = [collectionView cellForItemAtIndexPath:nextCellPath];
    if ([nextCell isKindOfClass:[LegacyAutofillEditCell class]])
      return nextCell;
    nextCellPath =
        [self indexPathWithSectionOffset:offset fromPath:nextCellPath];
  }
  return nil;
}

- (void)updateAccessoryViewButtonsStates {
  LegacyAutofillEditCell* previousCell = [self nextTextFieldWithOffset:-1];
  LegacyAutofillEditCell* nextCell = [self nextTextFieldWithOffset:1];
  [self.formInputAccessoryView.previousButton setEnabled:previousCell != nil];
  [self.formInputAccessoryView.nextButton setEnabled:nextCell != nil];
}

- (void)addOrRemoveErrorMessage:(NSString*)errorMessage
        inSectionWithIdentifier:(NSInteger)sectionIdentifier {
  CollectionViewModel* model = self.collectionViewModel;
  if ([model hasItemForItemType:ItemTypeErrorMessage
              sectionIdentifier:sectionIdentifier]) {
    NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeErrorMessage
                                       sectionIdentifier:sectionIdentifier];
    if (!errorMessage.length) {
      // Remove the item at the index path.
      [model removeItemWithType:ItemTypeErrorMessage
          fromSectionWithIdentifier:sectionIdentifier];
      [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];
    } else {
      // Reload the item at the index path.
      PaymentsTextItem* item = base::mac::ObjCCastStrict<PaymentsTextItem>(
          [model itemAtIndexPath:indexPath]);
      item.text = errorMessage;
      [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
    }
  } else if (errorMessage.length) {
    // Insert an item at the index path.
    [model addItem:ErrorMessageItemForError(errorMessage)
        toSectionWithIdentifier:sectionIdentifier];
    NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeErrorMessage
                                       sectionIdentifier:sectionIdentifier];
    [self.collectionView insertItemsAtIndexPaths:@[ indexPath ]];
  }
}

- (BOOL)validateField:(EditorField*)field {
  NSString* errorMessage =
      [_validatorDelegate paymentRequestEditViewController:self
                                             validateField:field];
  [self addOrRemoveErrorMessage:errorMessage
        inSectionWithIdentifier:field.sectionIdentifier];
  return errorMessage.length == 0;
}

- (BOOL)validateForm {
  for (EditorField* field in self.fields) {
    if (![self validateField:field]) {
      // Give the first invalid editor field focus, if possible
      if (field.fieldType == EditorFieldTypeTextField) {
        NSIndexPath* indexPath = [self.collectionViewModel
            indexPathForItemType:ItemTypeTextField
               sectionIdentifier:field.sectionIdentifier];
        id cell = [[self collectionView] cellForItemAtIndexPath:indexPath];
        // |cell| may be nil if the cell is not visible.
        if (cell) {
          LegacyAutofillEditCell* autofillEditCell =
              base::mac::ObjCCastStrict<LegacyAutofillEditCell>(cell);
          [autofillEditCell.textField becomeFirstResponder];
        }
      }
      return NO;
    }
  }
  return YES;
}

- (BOOL)isFieldValid:(EditorField*)field {
  NSString* errorMessage =
      [_validatorDelegate paymentRequestEditViewController:self
                                             validateField:field];
  return errorMessage.length == 0;
}

- (BOOL)isFormValid {
  for (EditorField* field in self.fields) {
    if (![self isFieldValid:field])
      return NO;
  }
  return YES;
}

- (NSIndexPath*)indexPathForCurrentTextField {
  DCHECK(_currentEditingCell);
  NSIndexPath* indexPath =
      [[self collectionView] indexPathForCell:_currentEditingCell];
  DCHECK(indexPath);
  return indexPath;
}

- (NSArray<NSArray<NSString*>*>*)pickerViewOptionsForPickerView:
    (UIPickerView*)pickerView {
  NSArray<NSNumber*>* keys = [self.pickerViews allKeysForObject:pickerView];
  DCHECK(keys.count == 1);
  return self.options[keys[0]];
}

#pragma mark - Keyboard handling

- (void)keyboardDidShow {
  [self.collectionView
      scrollToItemAtIndexPath:[self.collectionView
                                  indexPathForCell:_currentEditingCell]
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:YES];
}

#pragma mark Switch Actions

- (void)switchToggled:(UISwitch*)sender {
  CollectionViewSwitchCell* switchCell =
      CollectionViewSwitchCellForSwitchField(sender);
  NSIndexPath* indexPath = [[self collectionView] indexPathForCell:switchCell];
  DCHECK(indexPath);

  NSInteger sectionIdentifier = [self.collectionViewModel
      sectionIdentifierForSection:[indexPath section]];

  // Update editor field's value.
  NSNumber* key = [NSNumber numberWithInt:sectionIdentifier];
  EditorField* field = self.fieldsMap[key];
  DCHECK(field);
  field.value = [sender isOn] ? @"YES" : @"NO";
}

#pragma mark - PaymentRequestEditViewControllerActions methods

- (void)didCancel {
  [self.delegate paymentRequestEditViewControllerDidCancel:self];
}

- (void)didSave {
  [_currentEditingCell.textField resignFirstResponder];

  [self.delegate paymentRequestEditViewController:self
                           didFinishEditingFields:self.fields];
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self didCancel];
  return YES;
}

@end
