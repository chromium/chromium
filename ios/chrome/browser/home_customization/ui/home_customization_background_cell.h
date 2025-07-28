// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_

#import <UIKit/UIKit.h>

@protocol BackgroundCustomizationConfiguration;
@protocol HomeCustomizationMutator;
@class SearchEngineLogoMediator;

@class NewTabPageColorPalette;

// Represents a mini preview of how the NTP will look with a particular
// background selected. This cell is part of the background customization
// options, allowing users to interactively preview different background choices
// before applying them.
@interface HomeCustomizationBackgroundCell : UICollectionViewCell

// Mutator for communicating with the HomeCustomizationMediator.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Sets up and positions the view responsible for displaying the cell's
// content.
- (void)setupContentView:(UIView*)contentView;

// Configures the cell using the given background customization configuration.
// TODO(crbug.com/423883582): This class should not know
// `SearchEngineLogoMediator`.
- (void)configureWithBackgroundOption:
            (id<BackgroundCustomizationConfiguration>)backgroundConfiguration
             searchEngineLogoMediator:
                 (SearchEngineLogoMediator*)searchEngineLogoMediator
                         colorPalette:(NewTabPageColorPalette*)colorPalette;

// Updates the background image displayed behind the cell’s content.
- (void)updateBackgroundImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_H_
