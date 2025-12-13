// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_

#import <UIKit/UIKit.h>

@protocol BackgroundCustomizationConfiguration;
@class HomeCustomizationFramingCoordinates;
@protocol HomeCustomizationMutator;
@class NewTabPageColorPalette;
@class SearchEngineLogoMediator;

// Represents a mini preview of how the NTP will look with a particular
// background selected. This cell is part of the background customization
// options, allowing users to interactively preview different background choices
// before applying them.
@interface HomeCustomizationBackgroundCell : UICollectionViewCell

// Mutator for communicating with the HomeCustomizationMediator.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Main content view rendered inside the border wrapper.
// Displays the core visual element.
@property(nonatomic, strong) UIStackView* innerContentView;

// Sets up and positions the view responsible for displaying the cell's
// content.
- (void)setupContentView:(UIView*)contentView;

// Configures the cell using the given background customization configuration.
// TODO(crbug.com/436228514): This class should not know
// `SearchEngineLogoMediator`.
- (void)configureWithBackgroundOption:
            (id<BackgroundCustomizationConfiguration>)backgroundConfiguration
             searchEngineLogoMediator:
                 (SearchEngineLogoMediator*)searchEngineLogoMediator;

// Updates the background image displayed behind the cell’s content, using the
// provided framing coordinates to choose a sub-portion of the image to make
// visible.
- (void)updateBackgroundImage:(UIImage*)image
           framingCoordinates:
               (HomeCustomizationFramingCoordinates*)framingCoordinates;

// Applies the current theme.
- (void)applyTheme;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_
